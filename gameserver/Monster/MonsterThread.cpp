#include "MonsterThread.h"

#include "MonsterMgr.h"
#include "..\Debug.h"

#include "..\Session\SessionManager.h"

#include <chrono>
#include <thread>
///



std::atomic<bool> g_MonsterThreadRun{ false };

std::thread g_MonsterThread;

void MonsterThread()
{
    g_MonsterManager.CreateMonsters(20);

	while (g_MonsterThreadRun)
	{

		///chrono::high_resolution_clock::time_point frameStart;
        auto frameStart = std::chrono::high_resolution_clock::now();

        // 1. 이번 프레임에 업데이트할 몬스터들만 처리 (시분할)
        
		UpdateMonsters();

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




void UpdateMonsters()
{

    auto& monsters = g_MonsterManager.GetMonsters();
    if (monsters.empty()) return;

    // 시분할을 위해 static 인덱스 유지
    static int currentIdx = 0;
    int batchSize = 100; // 한 프레임(100ms)에 100마리씩 처리

    for (int i = 0; i < batchSize; ++i) {
        if (currentIdx >= monsters.size()) currentIdx = 0;

        Monster* monster = monsters[currentIdx++];

        // 간단한 랜덤 이동 AI 예시
        if (monster->hp > 0) {
            float dx = (rand() % 3 - 1) * 2.0f; // -2, 0, 2 중 하나
            float dy = (rand() % 3 - 1) * 2.0f;

            monster->pos.x = monster->pos.x + dx;
            monster->pos.y = monster->pos.y + dy;

            // 여기서 주변 유저가 있다면 이동 패킷 브로드캐스트!
            GSessionManager.BroadcastMonsterMove(monster);
        }


    }
}
