#include "MonsterThread.h"

#include "MonsterMgr.h"
#include "..\Debug.h"

#include "..\Session\SessionManager.h"
#include "..\GROUND_TILE\TileMgr.h"

#include "..\GROUND_TILE\SECTOR\SectorMgr.h"

#include <chrono>
#include <thread>
///



std::atomic<bool> g_MonsterThreadRun{ false };

std::thread g_MonsterThread;

void MonsterThread()
{

    g_pMonsterManager->CreateMonsters(10);

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



		g_pMonsterManager->UpdateMonsters(deltaTime);

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
