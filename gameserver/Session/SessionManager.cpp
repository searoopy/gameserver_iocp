#include <set>

#include "SessionManager.h"
#include "Session.h"
#include "..\Common.h"
#include "..\PROTOCOL\Protocol.h"
#include "..\Monster\Monster.h"
#include "..\Monster\MonsterMgr.h"
#include "..\GROUND_TILE\SECTOR\SectorMgr.h"
#include "..\PROTOCOL\PACKET.h"

std::unique_ptr < SessionManager> g_pSessionManager;

void SessionManager::Broadcast(OverlappedEx* sendOv) {
    std::vector<Session*> activeSessions = GetSessionsSnapshot();
    if (activeSessions.empty()) {
        GMemoryPool->Push(sendOv);
        return;
    }

    // [핵심 1] 방어용 카운트 1 설정 (내가 배포 중임을 표시)
    sendOv->refCount.store(1);

    // [핵심 2] 전송 가능한 대상 필터링
    std::vector<Session*> targets;
    for (Session* s : activeSessions) {
        if (s->isFree.load() || s->socket == INVALID_SOCKET || !s->isAuth) continue;
        targets.push_back(s);
    }

    if (targets.empty()) {
        GMemoryPool->Push(sendOv);
        return;
    }

    // [핵심 3] 전송 인원만큼 카운트 한꺼번에 증가
    sendOv->refCount.fetch_add((int32_t)targets.size());

    for (Session* session : targets) {
        // 복사본 생성 없이 원본(sendOv)을 그대로 전달
        SendPacket(session, sendOv);
    }

    // [핵심 4] 배포가 끝났으므로 방어용 1 제거
    // 마지막 전송 완료 스레드 혹은 여기서 0이 되어 안전하게 반납됨
    if (sendOv->refCount.fetch_sub(1) == 1) {
        GMemoryPool->Push(sendOv);
    }
}


void SessionManager::BroadcastNewUser(Session* newUser) 
{

    if (newUser == nullptr || !newUser->isAuth) return;

    // 1. 내가 속한 섹터 좌표 구하기
    Pos myIdx = g_pSectorMgr->GetSectorIndex(newUser->x, newUser->y);

    // [A] 주변 사람들에게 "나"를 생성하라고 알림 (공유 패킷 사용)
    OverlappedEx* myEnterOv = PACKET::CreateEnterPacket(newUser);
   // myEnterOv->Init();
    myEnterOv->type = IO_TYPE::SEND;
    // [중요] 일단 1을 세팅합니다. (내가 들고 있다는 증표)
    myEnterOv->refCount.store(1);

   // [개선] 9개 섹터 정보를 담을 통합 리스트
    std::vector<PlayerEntryInfo> totalList;
    totalList.reserve(30);


    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            Pos targetIdx = { myIdx.x + dx, myIdx.y + dy };
            if (!g_pSectorMgr->IsValidSector(targetIdx.x, targetIdx.y)) continue;

            // 주변에 나를 알림 (공유 패킷 사용)
            BroadcastToSector(targetIdx, myEnterOv, newUser->userUid);

            // [변경] 바로 보내지 말고 리스트에 정보만 수집하는 함수 호출
            CollectSectorMembers(targetIdx, newUser, totalList);
        }
    }

    // [핵심] 수집된 모든 정보를 딱 한 번의 Pop으로 전송!
    if (!totalList.empty()) {
        SendTotalMemberPacket(newUser, totalList);
    }

    // 아무도 주변에 없었다면 메모리 반납
    if (myEnterOv->refCount.fetch_sub(1) == 1) {
        GMemoryPool->Push(myEnterOv);
    }



}



void SessionManager::BroadcastLeaveUser(int32_t leavingUserUid, Pos lastPos ) {
   // if (leavingUser->userUid == -1) return;
    if (leavingUserUid == -1) return;


    //섹터에만 전송....
    Pos currentIdx = g_pSectorMgr->GetSectorIndex(lastPos.x, lastPos.y);

    // 공유 패킷 생성 (UID 기반)
    OverlappedEx* sharedOv = PACKET::CreateLeavePacket(leavingUserUid);
    sharedOv->refCount.store(1);

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            Pos targetIdx = { currentIdx.x + dx, currentIdx.y + dy };
            if (!g_pSectorMgr->IsValidSector(targetIdx.x, targetIdx.y)) continue;

            // 특정 UID를 제외하고 브로드캐스트
            BroadcastToSector(targetIdx, sharedOv, leavingUserUid);
        }
    }

    if (sharedOv->refCount.fetch_sub(1) == 1) {
        GMemoryPool->Push(sharedOv);
    }


}

void SessionManager::BroadcastMonsterMove(Monster* monster)
{
    if (monster == nullptr || m_connectedCount == 0) return;

    // [핵심 1] 공용 패킷 하나만 생성 (1명당 1개 X, 전체 1개 O)
    OverlappedEx* sharedOv = GMemoryPool->Pop();
    sharedOv->Init(); // 반드시 초기화! (refCount 0 -> 1로 설정됨을 가정)
    sharedOv->type = IO_TYPE::SEND;
    sharedOv->refCount.store(1); // 방어용 카운트 1 시작

    // 패킷 데이터 채우기
    S2C_MonsterMovePacket* pkt = reinterpret_cast<S2C_MonsterMovePacket*>(sharedOv->buffer);
    pkt->header.size = sizeof(S2C_MonsterMovePacket);
    pkt->header.id = (uint16_t)Packet_S2C::MONSTER_MOVE;
    pkt->monsterId = monster->monsterId;
    pkt->x = monster->pos.x.load();
    pkt->y = monster->pos.y.load();
    pkt->speed = monster->speed;

    std::vector<Session*> targets = GetSessionsSnapshot();
    const float VIEW_RANGE_SQ = 100000.0f * 100000.0f;

    // [핵심 2] 전송 대상 미리 선별
    std::vector<Session*> validTargets;
    for (Session* target : targets) {
        if (target->isFree.load() || target->socket == INVALID_SOCKET || !target->isAuth) continue;

        float diffX = pkt->x - target->x;
        float diffY = pkt->y - target->y;
        if ((diffX * diffX) + (diffY * diffY) <= VIEW_RANGE_SQ) {
            validTargets.push_back(target);
        }
    }

    if (!validTargets.empty()) {
        // [핵심 3] 보낼 인원만큼 한꺼번에 카운트 증가
        sharedOv->refCount.fetch_add((int32_t)validTargets.size());

        for (Session* target : validTargets) {
            SendPacket(target, sharedOv);
        }
    }

    // [핵심 4] 배포 종료. 방어용 1 제거 및 최종 반납 확인
    if (sharedOv->refCount.fetch_sub(1) == 1) {
        GMemoryPool->Push(sharedOv);
    }

}

void SessionManager::BroadcastAllLocations() {
    std::vector<PlayerPosInfo> movingPlayers;
    auto targets = GetSessionsSnapshot();

    for (Session* s : targets) {
        if (s->bPosChanged) {
            movingPlayers.push_back({ s->userUid, s->x, s->y });
            s->bPosChanged = false;
        }
    }
    if (movingPlayers.empty()) return;

    const int MAX_PLAYERS_PER_PACKET = 300;
    int totalMoving = static_cast<int>(movingPlayers.size());

    for (int i = 0; i < totalMoving; i += MAX_PLAYERS_PER_PACKET) {
        int countToSend = min(MAX_PLAYERS_PER_PACKET, totalMoving - i);
        uint16_t packetSize = sizeof(S2C_AllLocationPacket) + (sizeof(PlayerPosInfo) * countToSend);

        // [핵심] 이번 묶음을 보낼 공용 패킷 하나 생성
        OverlappedEx* sharedOv = GMemoryPool->Pop();
        sharedOv->Init();
        sharedOv->type = IO_TYPE::SEND;

        // [핵심 1] 방어용 카운트 1을 먼저 설정 (아직 아무에게도 안 보냈지만 내가 들고 있음)
        sharedOv->refCount.store(1);

        // 패킷 데이터 채우기
        S2C_AllLocationPacket* pkt = reinterpret_cast<S2C_AllLocationPacket*>(sharedOv->buffer);
        pkt->header.size = packetSize;
        pkt->header.id = static_cast<uint16_t>(Packet_S2C::ALL_LOCATE);
        pkt->playerCount = static_cast<uint16_t>(countToSend);
        memcpy(sharedOv->buffer + sizeof(S2C_AllLocationPacket), &movingPlayers[i], sizeof(PlayerPosInfo) * countToSend);

        // 1. 전송 대상 리스트업
        std::vector<Session*> validTargets;
        for (Session* target : targets) {
            if (target->isFree.load() || target->socket == INVALID_SOCKET) continue;
            validTargets.push_back(target);
        }

        // 2. 참조 카운트를 대상 인원수만큼 설정
        sharedOv->refCount.store((int32_t)validTargets.size());

        // 3. 각 세션에 전달
        for (Session* target : validTargets) {
            SendPacket(target, sharedOv);
        }

        // [핵심 4] 배포 예약이 끝났으므로 방어용 1을 제거
        // 만약 모든 전송이 순식간에 끝났다면 여기서 0이 되어 안전하게 반납됨
        if (sharedOv->refCount.fetch_sub(1) == 1) {
            GMemoryPool->Push(sharedOv);
        }


    }
}

void SessionManager::BroadcastMoveToNearby(Session* actor)
{
    // 1. 패킷 생성 (refCount 0으로 시작)
    OverlappedEx* movePkt = PACKET::CreateMovePacket(actor);
    movePkt->refCount.store(1);

    Pos currentIdx = g_pSectorMgr->GetSectorIndex(actor->x, actor->y);

    // 2. 주변 9개 섹터를 직접 순회하며 바로 Send
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            Pos targetIdx = { currentIdx.x + dx, currentIdx.y + dy };
            if (!g_pSectorMgr->IsValidSector(targetIdx.x, targetIdx.y)) continue;

            // BroadcastToSector 함수를 활용하면 코드 중복이 줄어듭니다.
            //  BroadcastToSector에 refCount 처리 로직이 있다면 사용)
            BroadcastToSector(targetIdx, movePkt );
        }
    }

    // 3. 아무도 주변에 없어서 전송이 안 됐다면 반납
    /*if (movePkt->refCount.load() == 0) {
        GMemoryPool->Push(movePkt);
    }*/
    if (movePkt->refCount.fetch_sub(1) == 1) {
        GMemoryPool->Push(movePkt);
    }
}

void SessionManager::BroadcastToSector(Pos sectorIdx, OverlappedEx* sharedOv, int exceptUid  )
{
    if (sharedOv == nullptr) return;

    std::vector<Session*> targets;
    {
        Sector& sector = g_pSectorMgr->GetSector(sectorIdx);
        std::shared_lock<std::shared_mutex> lock(sector.sessionLock);
        if (sector.sessions.empty()) return;
        targets = sector.sessions;
    }

    // 1. 실제로 보낼 대상들만 미리 필터링 (스냅샷 생성)
    std::vector<Session*> validTargets;
    validTargets.reserve(targets.size());

    for (Session* target : targets) {
        if (target->userUid == exceptUid || target->isFree.load() || !target->isAuth || target->socket == INVALID_SOCKET) {
            continue;
        }
        validTargets.push_back(target);
    }

    if (validTargets.empty()) return;

    // 2. [핵심] 보낼 인원수만큼 한꺼번에 카운트 증가 (Atomic 연산 최소화)
    sharedOv->refCount.fetch_add((int32_t)validTargets.size());

    // 3. 전송 시도
    for (Session* target : validTargets) {
        // SendPacket 내부에서는 refCount를 깎지 않도록 수정해야 합니다!
        SendPacket(target, sharedOv);
    }


}



void SessionManager::SendInitialMonsterLocations(Session* target)
{
    auto& monsters = g_pMonsterManager->GetMonsters();
    if (monsters.empty()) return;

    // 몬스터 위치 정보를 담을 벡터
    std::vector<PlayerPosInfo> monsterInfos;
    for (Monster* m : monsters) {
        if (m->hp > 0) {
            // UID를 몬스터용으로 구분해서 사용 (예: 10000번 이상)
            monsterInfos.push_back({ m->monsterId, m->pos.x, m->pos.y });
        }
    }

    // 패킷 분할 전송 (인원이 많을 경우 대비)
    const int MAX_PER_PACKET = 100;
    for (size_t i = 0; i < monsterInfos.size(); i += MAX_PER_PACKET) {
        int count = std::min<int>(MAX_PER_PACKET, (int)monsterInfos.size() - i);

        OverlappedEx* sendOv = GMemoryPool->Pop();
        if (sendOv == nullptr) return; // 방어 코드
        sendOv->Init(); // SLIST_ENTRY를 제외한 영역 초기화 (필수!)
        sendOv->type = IO_TYPE::SEND; // 워커 스레드가 이 타입을 보고 전송을 처리함
        sendOv->refCount.store(1);

        S2C_AllLocationPacket* pkt = reinterpret_cast<S2C_AllLocationPacket*>(sendOv->buffer);

        pkt->header.id = static_cast<uint16_t>(Packet_S2C::MONSTER_SPAWN); // 몬스터 스폰용 ID
        pkt->header.size = sizeof(S2C_AllLocationPacket) + (sizeof(PlayerPosInfo) * count);
        pkt->playerCount = count;

        memcpy(sendOv->buffer + sizeof(S2C_AllLocationPacket), &monsterInfos[i], sizeof(PlayerPosInfo) * count);

        SendPacket(target, sendOv);
    }
}







void SessionManager::SendSectorMembers(Session* me, Pos sectorIdx)
{
    // 1. 섹터 유저 스냅샷 추출
     std::vector<Session*> targets;
    {
        Sector& sector = g_pSectorMgr->GetSector(sectorIdx);
        std::shared_lock<std::shared_mutex> lock(sector.sessionLock);
        if (sector.sessions.empty()) return;
        targets = sector.sessions;
    }

    // 2. 나에게 보낼 '주변 유저 목록' 필터링
    std::vector<PlayerEntryInfo> playerList;
    playerList.reserve(targets.size());

    for (Session* other : targets) {
        if (other == me || other->isFree.load() || !other->isAuth) continue;
        playerList.push_back({ other->userUid, other->x, other->y, other->speed });
    }

    if (playerList.empty()) return;

    // --- [여기서부터 생략 없는 분할 전송 로직] ---

    const int MAX_PLAYERS_PER_PACKET = 50; // 안전하게 50명씩 끊어서 전송
    int totalPlayers = static_cast<int>(playerList.size());

    for (int i = 0; i < totalPlayers; i += MAX_PLAYERS_PER_PACKET) {
        // 이번 패킷에 담을 실제 인원 계산
        int count = std::min<int>(MAX_PLAYERS_PER_PACKET, totalPlayers - i);

        // 전송용 메모리 할당
        OverlappedEx* sendOv = GMemoryPool->Pop();
        if (sendOv == nullptr) {
            printf("Critical: Memory Pool is Empty during SectorMember Send!\n");
            break;
        }

        // 초기화 및 타입 설정
        sendOv->Init();
        sendOv->type = IO_TYPE::SEND;
        sendOv->refCount.store(1); // 나에게만 보내는 것이므로 1

        // 패킷 크기 및 헤더 설정
        uint16_t headerSize = sizeof(S2C_EnterPlayerListPacket);
        uint16_t dataSize = sizeof(PlayerEntryInfo) * count;
        uint16_t totalSize = headerSize + dataSize;

        // 버퍼 오버플로우 최종 방어 확인 (안전 장치)
        if (totalSize > MAX_BUFFER_SIZE) {
            printf("Error: Packet size %d exceeds MAX_BUFFER_SIZE!\n", totalSize);
            GMemoryPool->Push(sendOv);
            continue;
        }

        S2C_EnterPlayerListPacket* pkt = reinterpret_cast<S2C_EnterPlayerListPacket*>(sendOv->buffer);
        pkt->header.id = static_cast<uint16_t>(Packet_S2C::SECTOR_ENTER_PLAYER_LIST);
        pkt->header.size = totalSize;
        pkt->playerCount = static_cast<uint16_t>(count);

        // [핵심] playerList.data() + i 로 시작 지점을 옮겨가며 복사
        // playerList[i] 부터 count개만큼의 데이터를 복사합니다.
        memcpy(sendOv->buffer + headerSize,
            &playerList[i],
            dataSize);

        // 전송 시도
        SendPacket(me, sendOv);
    }
}

void SessionManager::SendSectorMembersLeave(Session* me, Pos sectorIdx)
{
    // 1. 해당 섹터 유저 스냅샷 추출
    std::vector<Session*> targets;
    {
        Sector& sector = g_pSectorMgr->GetSector(sectorIdx);
        std::shared_lock<std::shared_mutex> lock(sector.sessionLock);
        if (sector.sessions.empty()) return;
        targets = sector.sessions;
    }

    // 2. 삭제할 UID 리스트 수집
    std::vector<int32_t> leaveUids;
    leaveUids.reserve(targets.size());

    for (Session* other : targets) {
        // 본인 제외 및 유효한 세션만 수집
        if (other == me || other->isFree.load()) continue;
        leaveUids.push_back(other->userUid);
    }

    if (leaveUids.empty()) return;

    // --- [분할 전송 로직 시작] ---

    const int MAX_UIDS_PER_PACKET = 200; // UID는 데이터 크기가 작으므로 200개 정도가 적당
    int totalUids = static_cast<int>(leaveUids.size());

    for (int i = 0; i < totalUids; i += MAX_UIDS_PER_PACKET) {
        // 이번 패킷에 담을 UID 개수 계산
        int count = std::min<int>(MAX_UIDS_PER_PACKET, totalUids - i);

        // 메모리 할당 및 초기화
        OverlappedEx* sendOv = GMemoryPool->Pop();
        if (sendOv == nullptr) {
            printf("Critical: Memory Pool is Empty during SectorLeave Send!\n");
            break;
        }

        sendOv->Init();
        sendOv->type = IO_TYPE::SEND;
        sendOv->refCount.store(1); // 나에게만 보내는 것이므로 1

        // 패킷 크기 계산
        uint16_t headerSize = sizeof(S2C_LeavePlayerListPacket);
        uint16_t dataSize = sizeof(int32_t) * count;
        uint16_t totalSize = headerSize + dataSize;

        // 버퍼 크기 안전 체크
        if (totalSize > MAX_BUFFER_SIZE) {
            printf("Error: Leave Packet size %d exceeds MAX_BUFFER_SIZE!\n", totalSize);
            GMemoryPool->Push(sendOv);
            continue;
        }

        // 패킷 헤더 설정
        S2C_LeavePlayerListPacket* pkt = reinterpret_cast<S2C_LeavePlayerListPacket*>(sendOv->buffer);
        pkt->header.id = static_cast<uint16_t>(Packet_S2C::SECTOR_LEAVE_PLAYER_LIST);
        pkt->header.size = totalSize;
        pkt->playerCount = static_cast<uint16_t>(count);

        // [핵심] leaveUids.data() + i 지점부터 count개만큼 복사
        memcpy(sendOv->buffer + headerSize,
            &leaveUids[i],
            dataSize);

        // 전송 예약
        SendPacket(me, sendOv);
    }

}

void SessionManager::SendTotalMemberPacket(Session* target, std::vector<PlayerEntryInfo>& totalList)
{
    const int MAX_PER_PACKET = 80; // 버퍼 크기에 맞춰 조절 (안전하게 80~100명)
    int totalCount = static_cast<int>(totalList.size());

    for (int i = 0; i < totalCount; i += MAX_PER_PACKET) {
        int count = std::min<int>(MAX_PER_PACKET, totalCount - i);

        OverlappedEx* sendOv = GMemoryPool->Pop();
        if (sendOv == nullptr) {
            printf("Critical: Memory Pool Empty in SendTotalMemberPacket!\n");
            return;
        }

        sendOv->Init();
        sendOv->type = IO_TYPE::SEND;
        sendOv->refCount.store(1);

        uint16_t payloadSize = sizeof(PlayerEntryInfo) * count;
        uint16_t totalSize = sizeof(S2C_EnterPlayerListPacket) + payloadSize;

        // [방어 코드] 실제 버퍼 크기보다 큰지 최종 확인
        if (totalSize > MAX_BUFFER_SIZE) {
            GMemoryPool->Push(sendOv);
            continue;
        }

        S2C_EnterPlayerListPacket* pkt = reinterpret_cast<S2C_EnterPlayerListPacket*>(sendOv->buffer);
        pkt->header.id = static_cast<uint16_t>(Packet_S2C::SECTOR_ENTER_PLAYER_LIST);
        pkt->header.size = totalSize;
        pkt->playerCount = static_cast<uint16_t>(count);

        // 데이터 복사
        memcpy(sendOv->buffer + sizeof(S2C_EnterPlayerListPacket), &totalList[i], payloadSize);

        SendPacket(target, sendOv);
    }

}



void SessionManager::UpdateSssionMovement(float deltaTime)
{
    std::vector<Session*> targets = GetSessionsSnapshot();

  
    for (Session* target : targets)
    {

        if (!target->isMoving) continue;

        // 3. 개별 세션에 대해서만 락을 걸고 정밀 계산
        {
            std::lock_guard<std::mutex> lock(target->moveMutex);

            // 락을 잡은 후 다시 확인 (그 사이 멈췄을 수 있음)
            if (!target->isMoving || target->pathQueue.empty()) continue;

            // 실제 좌표 계산 로직 수행
            ProcessMovement(target, deltaTime);

        }
    }
}

void SessionManager::ProcessMovement(Session* session, float deltaTime)
{
    if (session == nullptr || !session->isAuth) return;


    // 1. 목적지가 없으면 이동 종료
    if (!session->isMoving || session->pathQueue.empty()) {
        session->isMoving = false;
        session->moveTimer = 0.0f;
        return;
    }

    // 2. 타이머 누적
    session->moveTimer += deltaTime;

    // 3. 한 칸 이동에 필요한 시간 계산 (예: speed가 5면 0.2초)
    float timePerTile = 1.0f / session->speed;

    while (session->moveTimer >= timePerTile) {
        if (session->pathQueue.empty()) break;

        Pos nextTile = session->pathQueue.front();

        // 1. 현재 위치와 같은 좌표가 큐에 들어있다면 즉시 제거하고 계속 진행
        if (nextTile.x == session->x && nextTile.y == session->y) {
            session->pathQueue.pop_front();
            if (session->pathQueue.empty()) {
                session->isMoving = false;
                break;
            }
            // pop만 하고 continue하면 다음 루프에서 바로 다음 칸 이동 시도
            continue;
        }

        // 2. 타이머 차감 및 실제 이동 시도
        session->moveTimer -= timePerTile;

        if (g_pTileMgr->TryOccupy(nextTile.x, nextTile.y, ENUM_TILE_NAME::player)) {
            g_pTileMgr->SetOccupied(session->x, session->y, ENUM_TILE_NAME::empty);


            int _oldx = session->x;
            int _oldy = session->y;
            session->x = nextTile.x;
            session->y = nextTile.y;

            // [중요] 좌표가 변했음을 마킹 (이동 종료 여부와 상관없이!)
            session->bPosChanged = true;





            session->pathQueue.pop_front();

            if (session->pathQueue.empty()) {
                session->isMoving = false;
                session->moveTimer = 0.0f;

                session->bPosChanged = true;

            }
           // session->bPosChanged = true; // 이동 성공 플래그


            //위치가 변했으니  섹터 인덱스 변화 확인후 전송....
            Pos oldIdx = g_pSectorMgr->GetSectorIndex(_oldx, _oldy);
            Pos newIdx = g_pSectorMgr->GetSectorIndex(session->x, session->y);

            if (oldIdx.x != newIdx.x || oldIdx.y != newIdx.y) {
                // [중요] 섹터 매니저 리스트 업데이트
                g_pSectorMgr->GetSector(oldIdx).Remove(session);
                g_pSectorMgr->GetSector(newIdx).Add(session);

                // [중요] 시야 동기화 (ENTER_LIST / LEAVE_LIST 전송)
                SyncView(session, oldIdx, newIdx);
            }
            else {
                // 섹터가 바뀌지 않았다면 주변 섹터(9개) 사람들에게 단순히 위치 변화만 알림
                // (이것은 기존의 BroadcastAllLocations나 개별 이동 패킷으로 처리)
                //BroadcastMoveToNearby(session);
            }



            break; // 한 프레임에 한 칸씩만 이동하려면 break
        }
        else {
            // 길막 처리
            session->isMoving = false;
            session->pathQueue.clear();
            session->moveTimer = 0.0f;

            session->bPosChanged = true; // 멈춘 위치라도 동기화하기 위해 설정
            break;
        }
    }


}


void SessionManager::SyncView(Session* session, Pos oldIdx, Pos newIdx) 
{
    // 나를 지우라는 패킷과 생성하라는 패킷을 각각 1개씩만 미리 생성
    OverlappedEx* leaveOv = PACKET::CreateLeavePacket(session);
    leaveOv->refCount.store(1); 

    OverlappedEx* enterOv = PACKET::CreateEnterPacket(session);
    enterOv->refCount.store(1);

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            // 1. 시야에서 사라지는 섹터 처리
            Pos outIdx = { oldIdx.x + dx, oldIdx.y + dy };
            if (g_pSectorMgr->IsValidSector(outIdx.x, outIdx.y)) {
                if (abs(outIdx.x - newIdx.x) > 1 || abs(outIdx.y - newIdx.y) > 1) {
                    SendSectorMembersLeave(session, outIdx); // 내 화면에서 삭제
                    BroadcastToSector(outIdx, leaveOv, session->userUid); // 남의 화면에서 나 삭제
                }
            }

            // 2. 시야에 새로 들어오는 섹터 처리
            Pos inIdx = { newIdx.x + dx, newIdx.y + dy };
            if (g_pSectorMgr->IsValidSector(inIdx.x, inIdx.y)) {
                if (abs(inIdx.x - oldIdx.x) > 1 || abs(inIdx.y - oldIdx.y) > 1) {

                   // printf("[SyncView] New Sector Enter! TargetSector(%d, %d) for Player(%d)\n", inIdx.x, inIdx.y, session->userUid);

                    SendSectorMembers(session, inIdx); // 내 화면에 생성
                    BroadcastToSector(inIdx, enterOv, session->userUid); // 남의 화면에 나 생성
                }
            }
        }
    }

    if (leaveOv->refCount.fetch_sub(1) == 1) {
        GMemoryPool->Push(leaveOv);
    }

    if (enterOv->refCount.fetch_sub(1) == 1) {
        GMemoryPool->Push(enterOv);
    }
}



void SessionManager::CollectSectorMembers(Pos sectorIdx, Session* me, std::vector<PlayerEntryInfo>& outList)
{
    Sector& sector = g_pSectorMgr->GetSector(sectorIdx);
    std::shared_lock<std::shared_mutex> lock(sector.sessionLock);

    for (Session* other : sector.sessions) {
        // 나 자신이 아니고, 인증되었으며, 해제되지 않은 유저만 수집
        if (other == me || !other->isAuth || other->isFree.load()) continue;

        outList.push_back({ other->userUid, other->x, other->y, other->speed });
    }
}



void SessionManager::SendPacket(Session* session, OverlappedEx* sendOv) {
    bool initiateSend = false;
    WSABUF wsaBufs[WSA_BUFF_CNT];
    int bufCount = 0;
    OverlappedEx* firstTarget = nullptr;

    {
        std::lock_guard<std::mutex> lock(session->sendMutex);
        if (session->isFree.load() || session->socket == INVALID_SOCKET) {
            
            // [주의] 만약 이 함수가 '공유 패킷'이 아닌 '단일 패킷' 전송용으로도 쓰인다면
            // 호출부에서 무조건 refCount를 1로 해서 보냈음을 보장해야 합니다.
            return;
        }

        session->sendQueue.push_back(sendOv);

        if (!session->isSending) {
            session->isSending = true;
            initiateSend = true;

            for (auto* ov : session->sendQueue) {
                if (bufCount >= WSA_BUFF_CNT) break;
                wsaBufs[bufCount].buf = ov->buffer;
                wsaBufs[bufCount].len = reinterpret_cast<PacketHeader*>(ov->buffer)->size;
                if (bufCount == 0) firstTarget = ov;
                bufCount++;
            }
            session->sendPendingCount = bufCount;
        }
    }

    if (initiateSend && firstTarget) {
        DWORD sentBytes = 0;
        if (WSASend(session->socket, wsaBufs, bufCount, &sentBytes, 0, (LPOVERLAPPED)&firstTarget->overlapped, NULL) == SOCKET_ERROR) {
            if (WSAGetLastError() != ERROR_IO_PENDING) {
                std::lock_guard<std::mutex> lock(session->sendMutex);
                //session->isSending = false;
                // 실패한 만큼 참조 카운트 복구/반납 로직이 필요할 수 있으나 보통 세션 종료 처리
               // session->sendPendingCount = 0;

                HandleDisconnect(session);
            }
        }
    }
}



void SessionManager::HandleDisconnect(Session* session) {
    if (session == nullptr) return;

    // 1. 중복 종료 방지
    bool expected = false;
    if (!session->isFree.compare_exchange_strong(expected, true)) {
        return;
    }

    // 2. 퇴장 알림을 위한 UID 백업 (세션이 초기화되기 전에 수행)
    int32_t backupUid = session->userUid;
    Pos lastPos = Pos(session->x, session->y);

    // 타일 설정

    if (session->isAuth) {
        g_pTileMgr->SetOccupied(session->x, session->y, ENUM_TILE_NAME::empty);

        // 이동 중이었다면 상태 초기화 (다른 쓰레드 참조 방지)
        session->isMoving = false;
        session->pathQueue.clear();
    }
  

    // 3. 소켓 닫기 (커널 신호 차단)
    if (session->socket != INVALID_SOCKET) {
        closesocket(session->socket);
        session->socket = INVALID_SOCKET;
    }


    // [추가] 3.5단계: 남아있는 전송 큐 정리
    {
        std::lock_guard<std::mutex> lock(session->sendMutex);
        while (!session->sendQueue.empty()) {
            OverlappedEx* ov = session->sendQueue.front();
            session->sendQueue.pop_front();

            // 참조 카운트를 깎고 마지막이면 반납
            if (ov->refCount.fetch_sub(1) == 1) {
                GMemoryPool->Push(ov);
            }
        }
        session->isSending = false;
        session->sendPendingCount = 0;
    }


    // 4. 활성 목록 제거 및 스택 반납
    // 내부에서 swap-back(O(1))으로 제거되고 프리 스택으로 들어갑니다.
    Release(session);

    // 5. [핵심] 다른 유저들에게 퇴장 알림 전송
    // Release 이후에 호출하여 목록 정리가 끝났음을 보장합니다.
    if (backupUid != -1) {
        BroadcastLeaveUser(backupUid,  lastPos );
    }
}