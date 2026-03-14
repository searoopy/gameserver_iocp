#include "MonsterThread.h"

#include "MonsterMgr.h"
#include "..\Debug.h"

#include "..\Session\SessionManager.h"
#include "..\GROUND_TILE\TileMgr.h"

#include <chrono>
#include <thread>
///



std::atomic<bool> g_MonsterThreadRun{ false };

std::thread g_MonsterThread;

void MonsterThread()
{
    g_pMonsterManager->CreateMonsters(80);
    auto lastTick = std::chrono::steady_clock::now();

	while (g_MonsterThreadRun)
	{

		///chrono::high_resolution_clock::time_point frameStart;
        auto frameStart = std::chrono::high_resolution_clock::now();

        // 1. 이번 프레임에 업데이트할 몬스터들만 처리 (시분할)
        auto currentTick = std::chrono::steady_clock::now();
        // 실제 경과 시간(deltaTime) 계산
        float deltaTime = std::chrono::duration<float>(currentTick - lastTick).count();
        lastTick = currentTick;



		UpdateMonsters(deltaTime);

        //LOG("몬스터 스레드 작동중...\n");


        // 2. 남은 시간만큼 정밀하게 Sleep
        auto frameEnd = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart);

		auto target = std::chrono::milliseconds(100); // 100ms = 10fps

        if (duration < target ) {
            std::this_thread::sleep_for(target - duration);
        }

	}


}


void StartMonsterThread() {

	if (g_MonsterThread.joinable()) {
		LOG("몬스터 스레드가 이미 실행 중입니다.\n");
		return;
	}

	g_MonsterThreadRun = true;
	g_MonsterThread = std::thread(MonsterThread);
}

void CleanMonsterThread() {
	g_MonsterThreadRun = false;

	if (g_MonsterThread.joinable()) {
		g_MonsterThread.join();
		//std::cout << "Monster Thread joined.\n";
	}
}

void UpdateMonsters(float deltaTime)
{
    auto& monsters = g_pMonsterManager->GetMonsters();
    if (monsters.empty()) return;

    std::vector<Session*> _sessions = g_SessionManager.GetSessionsSnapshot();

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

                // 1. 가장 가까운 유저 찾기
                Session* closestUser = nullptr;
                float minDistanceSq = 999999.0f; // 매우 큰 값으로 초기화

                int monsterX = monster->pos.x;
                int monsterY = monster->pos.y;

                for (Session* session : _sessions)
                {
                    if (!session->isAuth || session->isFree) continue;

                    // 유클리드 거리의 제곱: (x1-x2)^2 + (y1-y2)^2
                    float dx = (float)(session->x - monsterX);
                    float dy = (float)(session->y - monsterY);
                    float distSq = dx * dx + dy * dy;

                    if (distSq < minDistanceSq) {
                        minDistanceSq = distSq;
                        closestUser = session;
                    }
                }

                float aggroRangeSq = 10.0f * 10.0f;
                if (minDistanceSq <= aggroRangeSq) {
                    // 추격 로직 실행


                 // 2. 타겟 유저가 있다면 추격 로직 실행
                    if (closestUser != nullptr) {
                        int targetX = closestUser->x;
                        int targetY = closestUser->y;

                        int dx = targetX - monsterX;
                        int dy = targetY - monsterY;

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
                            g_pTileMgr->SetOccupied(monsterX, monsterY, ENUM_TILE_NAME::empty);
                            monster->pos.x = nextX;
                            monster->pos.y = nextY;

                            // 몬스터의 좌표가 변했으므로 브로드캐스트
                            g_SessionManager.BroadcastMonsterMove(monster);
                        }
                    }


                }
                else {
                    // 제자리 대기 혹은 랜덤 배회(Wander)
                }


            }
        }
    }
}