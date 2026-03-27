#include "MonsterMgr.h"

#include "..\Session\SessionManager.h"

#include "..\GROUND_TILE\SECTOR\SectorMgr.h"

#include "..\PROTOCOL\PACKET.h"
#include "..\GROUND_TILE\AStar.h"
#include "..\Common.h"


std::unique_ptr < MonsterManager> g_pMonsterManager;

void MonsterManager::CreateMonsters(int count)
{
	for (int i = 0; i < count; ++i)
	{
		int32_t mId = MONSTER_START_ID + i;
		Monster* monster = new Monster(mId);



		//램덤 위치 배치
		//monster->pos.x = 28;//static_cast<float>(rand() % 375);
		//monster->pos.y = 28;//static_cast<float>(rand() % 667);
		monster->pos.x = static_cast<int>(rand() % 50);
		monster->pos.y = static_cast<int>(rand() % 40);


        Pos startIdx = g_pSectorMgr->GetSectorIndex(monster->pos.x, monster->pos.y);
        g_pSectorMgr->GetSector(startIdx).AddMonster(monster);

		m_monsters.push_back(monster);

		// 여기서 주변 유저가 있다면 몬스터 입장 
		//g_pSessionManager->BroadcastToSector(startIdx, );
	}
}







void MonsterManager::UpdateMonsters(float deltaTime)
{
    auto& monsters = g_pMonsterManager->GetMonsters();
    if (monsters.empty()) return;

    //섹터 기반으로 변경.....
   // std::vector<Session*> _sessions = g_SessionManager.GetSessionsSnapshot();

    static int currentIdx = 0;
    int batchSize = 100;

    for (int i = 0; i < batchSize; ++i) {
        if (currentIdx >= monsters.size()) currentIdx = 0;
        Monster* monster = monsters[currentIdx++];

        if (monster->hp > 0)
        {



            monster->moveTimer += deltaTime;
            float timePerTile = 1.0f / monster->speed;

            if (monster->moveTimer >= timePerTile)
            {
                monster->moveTimer -= timePerTile;



                // [최적화 1] 현재 몬스터의 섹터 위치 파악  
                Pos curSectorIdx = g_pSectorMgr->GetSectorIndex(monster->pos.x, monster->pos.y);


                // 1. 가장 가까운 유저 찾기
                Session* closestUser = nullptr;
                float minDistanceSq = 999999.0f; // 매우 큰 값으로 초기화

                int monsterX = monster->pos.x;
                int monsterY = monster->pos.y;


                // 2. 주변 9개 섹터의 유저들을 한 번에 담을 임시 벡터
                std::vector<Session*> nearbySessions;

                // [변경점] 중첩 루프 대신 SectorManager의 함수를 호출하여 
                // 안전하게(shared_lock) 주변 유저 목록을 채웁니다.
                g_pSectorMgr->GetNearbySessions(monsterX, monsterY, nearbySessions);

            
                // 3. 수집된 유저들만 전수 조사 (전체 접속자 X, 주변 유저 O)
                for (Session* session : nearbySessions)
                {
                    // 세션 유효성 검사
                    if (!session || !session->isAuth || session->isFree) continue;

                    // 거리 계산 (제곱 비교가 루트 연산보다 빠름)
                    float dx = (float)(session->x - monsterX);
                    float dy = (float)(session->y - monsterY);
                    float distSq = dx * dx + dy * dy;

                    if (distSq < minDistanceSq) {
                        minDistanceSq = distSq;
                        closestUser = session;
                    }
                }


                float aggroRangeSq = 10.0f * 10.0f;
                if (minDistanceSq <= aggroRangeSq)
                {
                    // 추격 로직 실행
                    ProcessMonsterChase(monster, closestUser);

                }
                else {
                    // 제자리 대기 혹은 랜덤 배회(Wander)
                }


            }
        }
    }
}


void MonsterManager::ProcessMonsterChase(Monster* monster, Session* closestUser)
{
    // 2. 타겟 유저가 있다면 추격 로직 실행
    if (closestUser != nullptr) {
        int targetX = closestUser->x;
        int targetY = closestUser->y;
        int monsterX = monster->pos.x;
        int monsterY = monster->pos.y;


        int dx = targetX - monsterX;
        int dy = targetY - monsterY;

        // 이미 타겟과 같은 위치라면 공격 등의 다른 처리를 위해 리턴
         if (dx == 0 && dy == 0) return;

        int nextX = monsterX;
        int nextY = monsterY;
        bool moved = false;

        // X축 이동 시도
        if (dx != 0) {
            int stepX = (dx > 0) ? 1 : -1;
            if (g_pTileMgr->TryOccupy(monsterX + stepX, monsterY, ENUM_TILE_NAME::monster)) {
                nextX = monsterX + stepX;
                moved = true;
            }
        }

        // Y축 이동 시도 (X축이 막혔거나 이미 X축 위치가 같을 때)
        if (!moved && dy != 0) {
            int stepY = (dy > 0) ? 1 : -1;
            if (g_pTileMgr->TryOccupy(monsterX, monsterY + stepY, ENUM_TILE_NAME::monster)) {
                nextY = monsterY + stepY;
                moved = true;
            }
        }

        if (moved) {
            //g_pTileMgr->SetOccupied(monsterX, monsterY, ENUM_TILE_NAME::empty);
            //monster->pos.x = nextX;
            //monster->pos.y = nextY;

            //// 몬스터의 좌표가 변했으므로 브로드캐스트
          
            //g_SessionManager.BroadcastMonsterMove(monster);

            ProcessMonsterMove(monster, nextX, nextY);


        }
    }

}

void MonsterManager::ProcessMonsterMove(Monster* m, int nextX, int nextY)
{
    // 이동 전/후의 섹터 인덱스 계산
    Pos oldIdx = g_pSectorMgr->GetSectorIndex(m->pos.x, m->pos.y);
    Pos newIdx = g_pSectorMgr->GetSectorIndex(nextX, nextY);

    // 1. 타일 정보 업데이트
    g_pTileMgr->SetOccupied(m->pos.x, m->pos.y, ENUM_TILE_NAME::empty);

    m->pos.x = nextX;
    m->pos.y = nextY;

    // 2. 섹터 변경 확인
    if (oldIdx.x != newIdx.x || oldIdx.y != newIdx.y) {
        // [섹터 관리] 이전 섹터에서 제거, 새 섹터에 추가
        g_pSectorMgr->GetSector(oldIdx).RemoveMonster(m);
        g_pSectorMgr->GetSector(newIdx).AddMonster(m);

        // [시야 동기화] 새로운 구역 유저에겐 ENTER, 멀어진 유저에겐 LEAVE
        SyncMonsterView(m, oldIdx, newIdx);
    }
    else {
        // 섹터가 바뀌지 않았다면 주변 9개 섹터 유저들에게만 이동 패킷 전송
        BroadcastMonsterMoveToNearby(m);
    }

}

void MonsterManager::BroadcastMonsterMoveToNearby(Monster* actor)
{
    if (!actor) return;

    // 1. 현재 몬스터가 속한 섹터 좌표 구하기
    Pos currentIdx = g_pSectorMgr->GetSectorIndex(actor->pos.x, actor->pos.y);

    // 2. 몬스터 이동 패킷 생성 (참조 카운팅 활용)
    // PACKET::CreateMonsterMovePacket 등 몬스터 전용 패킷 생성 함수 필요
    OverlappedEx* movePkt = PACKET::CreateMonsterMovePacket(actor);
    movePkt->refCount.store(1); // [방어막 1단계] 내가 배포 중임을 표시


    // 3. 주변 9개 섹터를 순회하며 전송 대상(Session) 수집
    std::vector<Session*> totalTargets;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            Pos targetIdx = { currentIdx.x + dx, currentIdx.y + dy };

            if (!g_pSectorMgr->IsValidSector(targetIdx.x, targetIdx.y)) continue;

            Sector& sector = g_pSectorMgr->GetSector(targetIdx);

            // [중요] 세션 리스트 접근을 위해 sessionLock(shared_mutex) 사용
            std::shared_lock<std::shared_mutex> lock(sector.sessionLock);
            if (sector.sessions.empty()) continue;

            // 해당 섹터에 있는 플레이어(Session)들에게 보낼 준비
            totalTargets.insert(totalTargets.end(), sector.sessions.begin(), sector.sessions.end());
        }
    }

    // 전송 대상이 없으면 패킷 즉시 반납
    if (totalTargets.empty()) {
        GMemoryPool->Push(movePkt);
        return;
    }

    // 4. 참조 카운트 설정 및 전송 (플레이어 로직과 동일)
    movePkt->refCount.store(static_cast<int32_t>(totalTargets.size()));

    for (Session* target : totalTargets) {
        // 비인증 유저나 종료된 세션 필터링
        if (target->isFree.load() || !target->isAuth) {
            if (movePkt->refCount.fetch_sub(1) == 1) {
                GMemoryPool->Push(movePkt);
            }
            continue;
        }

        // 실제 전송 (기존 SessionManager의 SendPacket이나 공용 Send 함수 활용)
        g_pSessionManager->SendPacket(target, movePkt);
    }

    // [방어막 3단계] 배포 지시 종료. 내가 잡고 있던 1을 제거
    if (movePkt->refCount.fetch_sub(1) == 1) {
        GMemoryPool->Push(movePkt);
    }

}

void MonsterManager::BroadcastToList(const std::vector<Session*>& targets, OverlappedEx* pkt)
{
    if (targets.empty()) {
        GMemoryPool->Push(pkt);
        return;
    }



    pkt->refCount.store(1);


    pkt->refCount.fetch_add(static_cast<int32_t>(targets.size()));

    for (Session* target : targets) {
        if (!target->isAuth || target->isFree.load()) {
            if (pkt->refCount.fetch_sub(1) == 1) {
                GMemoryPool->Push(pkt);
            }
            continue;
        }

        g_pSessionManager->SendPacket(target, pkt);


    }

    if (pkt->refCount.fetch_sub(1) == 1) {
        GMemoryPool->Push(pkt);
    }

}



void MonsterManager::SyncMonsterView(Monster* monster,const  Pos& oldIdx,const  Pos& newIdx)
{

    if (!monster) return;

    // 1 & 2. 주변 세션 가져오기 및 정렬
    std::vector<Session*> oldNearby;
    g_pSectorMgr->GetNearbySessions(oldIdx.x, oldIdx.y, oldNearby);
    std::vector<Session*> newNearby;
    g_pSectorMgr->GetNearbySessions(newIdx.x, newIdx.y, newNearby);

    std::sort(oldNearby.begin(), oldNearby.end());
    std::sort(newNearby.begin(), newNearby.end());

    // 3. LEAVE 처리 (멀어진 유저들)
    std::vector<Session*> leaveList;
    std::set_difference(oldNearby.begin(), oldNearby.end(),
        newNearby.begin(), newNearby.end(),
        std::back_inserter(leaveList));

    if (!leaveList.empty()) {
        OverlappedEx* leavePkt = PACKET::CreateMonsterLeavePacket(monster);
        BroadcastToList(leaveList, leavePkt);
    }

    // 4. ENTER 처리 (새로 발견한 유저들)
    std::vector<Session*> enterList;
    std::set_difference(newNearby.begin(), newNearby.end(),
        oldNearby.begin(), oldNearby.end(),
        std::back_inserter(enterList));

    if (!enterList.empty()) {
        OverlappedEx* enterPkt = PACKET::CreateMonsterEnterPacket(monster);
        BroadcastToList(enterList, enterPkt);
    }

    // 5. MOVE 처리 (계속 시야에 있는 유저들)
    std::vector<Session*> moveList;
    std::set_intersection(oldNearby.begin(), oldNearby.end(),
        newNearby.begin(), newNearby.end(),
        std::back_inserter(moveList));

    if (!moveList.empty()) {
        OverlappedEx* movePkt = PACKET::CreateMonsterMovePacket(monster);
        BroadcastToList(moveList, movePkt);
    }
}