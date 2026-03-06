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
    g_MonsterManager.CreateMonsters(20);
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

        LOG("몬스터 스레드 작동중...\n");


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



//몬스터 스레드는 싱글스레드임....(참고)
void UpdateMonsters(float deltaTime)
{

    auto& monsters = g_MonsterManager.GetMonsters();
    if (monsters.empty()) return;


    std::vector<Session*> _sessions =  g_SessionManager.GetSessionsSnapshot();


    // 시분할을 위해 static 인덱스 유지
    static int currentIdx = 0;
    int batchSize = 100; // 한 프레임(100ms)에 100마리씩 처리

    for (int i = 0; i < batchSize; ++i) {
        if (currentIdx >= monsters.size()) currentIdx = 0;

        Monster* monster = monsters[currentIdx++];

        // 간단한 랜덤 이동 AI 예시
        if (monster->hp > 0)
        {

            monster->moveTimer += deltaTime;

            //  한 칸 이동에 필요한 시간 계산 (예: speed가 5면 0.2초)
            float timePerTile = 1.0f / monster->speed;

            //이동할 시간이 되면 이동.....
            if (monster->moveTimer >= timePerTile) 
            {

                monster->moveTimer -= timePerTile;

                //가까운 유저 추적...
                for (Session* session : _sessions)
                {
                    // 2. 인증된 유저인지, 살아있는지 확인
                    if (!session->isAuth || session->isFree) continue;





                    int targetX = session->x;
                    int targetY = session->y;
                    int monsterX = monster->pos.x;
                    int monsterY = monster->pos.y;

                    int nextX = monsterX;
                    int nextY = monsterY;

                    // 단순 추적 로직 (장애물 고려 X)
                    if (targetX > monsterX) nextX++;
                    else if (targetX < monsterX) nextX--;
                    else if (targetY > monsterY) nextY++;
                    else if (targetY < monsterY) nextY--;

                    if (g_tileMgr.IsWalkable(nextX, nextY) && !g_tileMgr.IsOccupied(nextX, nextY)) {
                        // 기존 타일 점유 해제
                        g_tileMgr.SetOccupied(monsterX, monsterY, ENUM_TILE_NAME::empty);

                        // 새로운 타일 점유 및 이동
                        monster->pos.x = nextX;
                        monster->pos.y = nextY;
                        g_tileMgr.SetOccupied(nextX, nextY, ENUM_TILE_NAME::monster);

                    }//갈수있는 타일인지 검사 끝.

                }




            }



            // 여기서 주변 유저가 있다면 이동 패킷 브로드캐스트!
            g_SessionManager.BroadcastMonsterMove(monster);

        }///hp >0 


    }
}
