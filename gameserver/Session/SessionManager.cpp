#include "SessionManager.h"
#include "Session.h"
#include "..\Common.h"
#include "..\PROTOCOL\Protocol.h"
#include "..\Monster\Monster.h"
#include "..\Monster\MonsterMgr.h"

SessionManager g_SessionManager;


void SessionManager::Broadcast(OverlappedEx* sendOv) {
    std::vector<Session*> activeSessions = GetSessionsSnapshot();

    for (Session* session : activeSessions) {
        // 이미 종료 처리 중인 세션은 건너뜁니다.
        if (session->isFree.load()) continue;

        // 새로운 OverlappedEx 할당
        OverlappedEx* copyOv = GMemoryPool->Pop();

        // [중요] 전체 memcpy 대신 필요한 데이터만 복사
        // OVERLAPPED 구조체 영역은 0으로 초기화되어야 합니다.
        memset(copyOv, 0, sizeof(OverlappedEx));

        copyOv->type = sendOv->type;
        copyOv->sessionSocket = session->socket; // 대상 세션의 소켓으로 설정

        // 패킷 헤더를 통해 실제 복사할 크기 계산 (전체 버퍼 복사보다 효율적)
        PacketHeader* header = reinterpret_cast<PacketHeader*>(sendOv->buffer);
        memcpy(copyOv->buffer, sendOv->buffer, header->size);

        // 전송 시도
        SendPacket(session, copyOv);
    }

    // 원본은 반납
    GMemoryPool->Push(sendOv);
}


void SessionManager::BroadcastNewUser(Session* newUser) {

    std::vector<Session*> activeSessions = GetSessionsSnapshot();

    // 2. 루프 밖에서 패킷 생성 (공용 데이터)
    OverlappedEx* sendOv = GMemoryPool->Pop();
    memset(sendOv, 0, sizeof(OverlappedEx));
    sendOv->type = IO_TYPE::SEND;

    S2C_EnterUserPacket* pkt = reinterpret_cast<S2C_EnterUserPacket*>(sendOv->buffer);
    pkt->header.size = sizeof(S2C_EnterUserPacket);
    pkt->header.id = static_cast<uint16_t>(Packet_S2C::ENTER_USER);
    pkt->userUid = newUser->userUid;
    pkt->x = newUser->x;
    pkt->y = newUser->y;

    // 3. 락이 없는 상태에서 안전하게 브로드캐스트 수행
    for (Session* other : activeSessions) {
        if (other == newUser) continue;
        if (other->isFree) continue; // 그 사이 나갔을 경우를 대비한 방어 코드

        // [A] 기존 유저에게 새 유저 소식 알림
        OverlappedEx* copyOv = GMemoryPool->Pop();
        memcpy(copyOv, sendOv, sizeof(OverlappedEx));
        SendPacket(other, copyOv);

        // [B] 새 유저에게 기존 유저(other)의 정보 전송
        OverlappedEx* existingUserOv = GMemoryPool->Pop();
        memset(existingUserOv, 0, sizeof(OverlappedEx));
        existingUserOv->type = IO_TYPE::SEND;

        S2C_EnterUserPacket* ePkt = reinterpret_cast<S2C_EnterUserPacket*>(existingUserOv->buffer);
        ePkt->header.size = sizeof(S2C_EnterUserPacket);
        ePkt->header.id = static_cast<uint16_t>(Packet_S2C::ENTER_USER);
        ePkt->userUid = other->userUid;
        ePkt->x = other->x;
        ePkt->y = other->y;

        SendPacket(newUser, existingUserOv);
    }

    GMemoryPool->Push(sendOv); // 원본 반납
}



void SessionManager::BroadcastLeaveUser(int32_t leavingUserUid) {
   // if (leavingUser->userUid == -1) return;
    if (leavingUserUid == -1) return;



    //1.저ㅗㄴ송할 패킷 구성
    S2C_LeaveUserPacket leavePkt;
    leavePkt.header.id = static_cast<uint16_t>(Packet_S2C::LEAVE_USER);
    leavePkt.header.size = sizeof(S2C_LeaveUserPacket);
    leavePkt.userUid = leavingUserUid;

    // 2. 락 경합 최소화를 위한 스냅샷 생성
    // m_activeMutex를 아주 잠깐만 잡고 리스트를 복사합니다.
    std::vector<Session*> targets = GetSessionsSnapshot();


    // 2. 현재 접속 중인 모든 유저에게 전송 (스냅샷 활용)
    // 원본 패킷(sendOv)을 Broadcast 함수에 넣어서 처리
   // this->Broadcast(sendOv);
    // 3. 전송 루프
    for (Session* target : targets) {
        // 유효성 체크 및 자기 자신 제외
        if (target->isFree.load() || target->userUid == leavingUserUid)
            continue;

        // 전송용 OverlappedEx 생성
        OverlappedEx* sendOv = GMemoryPool->Pop();
        if (sendOv == nullptr) continue;

        memset(sendOv, 0, sizeof(OverlappedEx));
        sendOv->type = IO_TYPE::SEND;
        memcpy(sendOv->buffer, &leavePkt, sizeof(S2C_LeaveUserPacket));

      
        // Gather Write가 적용된 SendPacket 호출
        // 이제 120명 상황에서도 시스템 콜 부담 없이 순식간에 알림이 전달됩니다.
        SendPacket(target, sendOv);
    }


}

void SessionManager::BroadcastMonsterMove(Monster* monster)
{
    if (monster == nullptr) return;

    //접속자가 없으면 안보냄.
    if (m_connectedCount == 0) return;

    // 1. 패킷 데이터 준비
    S2C_MonsterMovePacket pkt;
    pkt.header.size = sizeof(S2C_MonsterMovePacket);
    pkt.header.id = (uint16_t)Packet_S2C::MONSTER_MOVE;
    pkt.userUid = -1;
    pkt.monsterId = monster->monsterId;
    pkt.x = monster->pos.x.load(); // atomic 읽기
    pkt.y = monster->pos.y.load();

    // 2. 유저 목록 스냅샷 (락 최소화)
    std::vector<Session*> targets = GetSessionsSnapshot();

    // 3. 거리 기반 필터링 및 전송
    const float VIEW_RANGE = 100000.0f; // 몬스터를 볼 수 있는 거리 (픽셀 단위)

    for (Session* target : targets) {
        // 세션 유효성 체크
        if (target->isFree.load() || target->socket == INVALID_SOCKET || target->isAuth == false )
            continue;

        // [핵심] 거리 체크 (피타고라스 정리)
        float diffX = pkt.x - target->x;
        float diffY = pkt.y - target->y;
        float distSq = (diffX * diffX) + (diffY * diffY);



        //근처 플레이어에게만 보냄.
        if (distSq <= VIEW_RANGE * VIEW_RANGE) {
            // 전송용 OverlappedEx 할당
            OverlappedEx* sendOv = GMemoryPool->Pop();
            if (sendOv == nullptr) continue;

            memset(sendOv, 0, sizeof(OverlappedEx));
            sendOv->type = IO_TYPE::SEND;
            memcpy(sendOv->buffer, &pkt, sizeof(S2C_MonsterMovePacket));

            // Gather Write가 적용된 SendPacket 호출
            SendPacket(target, sendOv);
        }
    }

}


/*
void SessionManager::BroadcastAllLocations()
{
    std::vector<PlayerPosInfo> movingPlayers;
    std::vector<Session*> targets = GetSessionsSnapshot();

    for (Session* s : targets) {
        //if (s->isMoving) {
        if (s->bPosChanged) {
           
            movingPlayers.push_back({ s->userUid, s->x, s->y });
            s->bPosChanged = false;

        }
    }

    if (movingPlayers.empty()) return;

    // --- 패킷 분할 로직 ---
    const int MAX_PLAYERS_PER_PACKET = 300; // 안전하게 300명씩 끊어서 전송
    int totalMoving = static_cast<int>(movingPlayers.size());

    for (int i = 0; i < totalMoving; i += MAX_PLAYERS_PER_PACKET) {
        int countToSend = min(MAX_PLAYERS_PER_PACKET, totalMoving - i);
        uint16_t packetSize = sizeof(S2C_AllLocationPacket) + (sizeof(PlayerPosInfo) * countToSend);

        for (Session* target : targets) {
            if (target->isFree.load() || target->socket == INVALID_SOCKET) continue;

            OverlappedEx* sendOv = GMemoryPool->Pop();
            S2C_AllLocationPacket* pkt = reinterpret_cast<S2C_AllLocationPacket*>(sendOv->buffer);

            pkt->header.size = packetSize;
            pkt->header.id = static_cast<uint16_t>(Packet_S2C::ALL_LOCATE);
            pkt->playerCount = static_cast<uint16_t>(countToSend);

            // i번째 인덱스부터 countToSend만큼 복사
            memcpy(sendOv->buffer + sizeof(S2C_AllLocationPacket),
                &movingPlayers[i],
                sizeof(PlayerPosInfo) * countToSend);

            SendPacket(target, sendOv);
        }
    }
}
*/
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
    }
}



void SessionManager::SendInitialMonsterLocations(Session* target)
{
    auto& monsters = g_MonsterManager.GetMonsters();
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
    const int MAX_PER_PACKET = 300;
    for (size_t i = 0; i < monsterInfos.size(); i += MAX_PER_PACKET) {
        int count = std::min<int>(MAX_PER_PACKET, (int)monsterInfos.size() - i);

        OverlappedEx* sendOv = GMemoryPool->Pop();
        S2C_AllLocationPacket* pkt = reinterpret_cast<S2C_AllLocationPacket*>(sendOv->buffer);

        pkt->header.id = static_cast<uint16_t>(Packet_S2C::MONSTER_SPAWN); // 몬스터 스폰용 ID
        pkt->header.size = sizeof(S2C_AllLocationPacket) + (sizeof(PlayerPosInfo) * count);
        pkt->playerCount = count;

        memcpy(sendOv->buffer + sizeof(S2C_AllLocationPacket), &monsterInfos[i], sizeof(PlayerPosInfo) * count);

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
            session->x = nextTile.x;
            session->y = nextTile.y;


            // [중요] 좌표가 변했음을 마킹 (이동 종료 여부와 상관없이!)
            session->bPosChanged = true;


            session->pathQueue.pop_front();

            if (session->pathQueue.empty()) {
                session->isMoving = false;
                session->moveTimer = 0.0f;
            }
           // session->bPosChanged = true; // 이동 성공 플래그
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


//void SendPacket(Session* session, OverlappedEx* sendOv) {
//    bool failed = false;
//    bool initiateSend = false;
//
//    // Gather 전송을 위한 준비
//    WSABUF wsaBufs[WSA_BUFF_CNT];
//    int bufCount = 0;
//    OverlappedEx* firstTarget = nullptr;
//
//    {
//        std::lock_guard<std::mutex> lock(session->sendMutex);
//
//        if (session->isFree.load() || session->socket == INVALID_SOCKET) {
//            GMemoryPool->Push(sendOv);
//            return;
//        }
//
//        // 1. 일단 큐에 넣기
//        session->sendQueue.push_back(sendOv);
//
//        // 2. 현재 전송 중이 아니라면, 큐에 쌓인 것들을 최대한 긁어서 전송 시작
//        if (!session->isSending) {
//            session->isSending = true;
//            initiateSend = true;
//
//            // 큐에 있는 패킷들을 최대 WSA_BUFF_CNT(32)개까지 Gather
//            for (auto* ov : session->sendQueue) {
//                if (bufCount >= WSA_BUFF_CNT) break;
//
//                wsaBufs[bufCount].buf = ov->buffer;
//                wsaBufs[bufCount].len = reinterpret_cast<PacketHeader*>(ov->buffer)->size;
//
//                if (bufCount == 0) firstTarget = ov;
//                bufCount++;
//            }
//
//            // [중요] 이번 전송에 포함된 패킷 개수를 기록!
//            session->sendPendingCount = bufCount;
//        }
//    }
//
//    // 3. 락 밖에서 전송 호출
//    if (initiateSend && firstTarget) {
//        DWORD sentBytes = 0;
//        // bufCount 만큼의 배열을 한 번에 보냄
//        if (WSASend(session->socket, wsaBufs, bufCount, &sentBytes, 0, (LPOVERLAPPED)firstTarget, NULL) == SOCKET_ERROR) {
//            int errCode = WSAGetLastError();
//            if (errCode != ERROR_IO_PENDING) {
//                {
//                    std::lock_guard<std::mutex> lock(session->sendMutex);
//                    session->isSending = false;
//                    session->sendPendingCount = 0; // 실패했으므로 카운트 리셋
//                }
//                failed = true;
//                // HandleDisconnect(session); // 필요 시 호출
//            }
//        }
//    }
//}


/*
void SendPacket(Session* session, OverlappedEx* sendOv) {
    // 1. 락 구간을 극도로 짧게 가져갑니다.
    bool expected = false;
    bool initiateSend = false;

    {
        std::lock_guard<std::mutex> lock(session->sendMutex);
        if (session->isFree || session->socket == INVALID_SOCKET) {
            GMemoryPool->Push(sendOv);
            return;
        }

        session->sendQueue.push_back(sendOv);

        // 전송 중이 아닐 때만 '딱 한 번' 플래그를 세웁니다.
        if (!session->isSending) {
            session->isSending = true;
            initiateSend = true;
        }
    }

    // 2. 만약 내가 전송 시작 권한을 얻었다면, 락 밖에서 큐를 훑습니다.
    if (initiateSend) {
        WSABUF wsaBufs[WSA_BUFF_CNT];
        int bufCount = 0;
        OverlappedEx* firstTarget = nullptr;

        // 락을 다시 잡되, 이번에는 전송에 필요한 정보만 '복사'해옵니다.
        {
            std::lock_guard<std::mutex> lock(session->sendMutex);
            for (auto* ov : session->sendQueue) {
                if (bufCount >= WSA_BUFF_CNT) break;
                wsaBufs[bufCount].buf = ov->buffer;
                wsaBufs[bufCount].len = reinterpret_cast<PacketHeader*>(ov->buffer)->size;
                if (bufCount == 0) firstTarget = ov;
                bufCount++;
            }
            session->sendPendingCount = bufCount;
        }

        if (firstTarget) {
            DWORD sentBytes = 0;
            if (WSASend(session->socket, wsaBufs, bufCount, &sentBytes, 0, (LPOVERLAPPED)firstTarget, NULL) == SOCKET_ERROR) {
                if (WSAGetLastError() != ERROR_IO_PENDING) {
                    std::lock_guard<std::mutex> lock(session->sendMutex);
                    session->isSending = false;
                    session->sendPendingCount = 0;
                }
            }
        }
    }
}
*/

void SendPacket(Session* session, OverlappedEx* sendOv) {
    bool initiateSend = false;
    WSABUF wsaBufs[WSA_BUFF_CNT];
    int bufCount = 0;
    OverlappedEx* firstTarget = nullptr;

    {
        std::lock_guard<std::mutex> lock(session->sendMutex);
        if (session->isFree.load() || session->socket == INVALID_SOCKET) {
            // 대상이 유효하지 않으면 카운트 깎고 0이면 반납
            if (sendOv->refCount.fetch_sub(1) == 1) GMemoryPool->Push(sendOv);
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
        if (WSASend(session->socket, wsaBufs, bufCount, &sentBytes, 0, (LPOVERLAPPED)firstTarget, NULL) == SOCKET_ERROR) {
            if (WSAGetLastError() != ERROR_IO_PENDING) {
                std::lock_guard<std::mutex> lock(session->sendMutex);
                session->isSending = false;
                // 실패한 만큼 참조 카운트 복구/반납 로직이 필요할 수 있으나 보통 세션 종료 처리
                session->sendPendingCount = 0;
            }
        }
    }
}



void HandleDisconnect(Session* session) {
    if (session == nullptr) return;

    // 1. 중복 종료 방지
    bool expected = false;
    if (!session->isFree.compare_exchange_strong(expected, true)) {
        return;
    }

    // 2. 퇴장 알림을 위한 UID 백업 (세션이 초기화되기 전에 수행)
    int32_t backupUid = session->userUid;

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
    g_SessionManager.Release(session);

    // 5. [핵심] 다른 유저들에게 퇴장 알림 전송
    // Release 이후에 호출하여 목록 정리가 끝났음을 보장합니다.
    if (backupUid != -1) {
        g_SessionManager.BroadcastLeaveUser(backupUid);
    }
}