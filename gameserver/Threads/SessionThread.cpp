#include "SessionThread.h"
#include <WinSock2.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <thread>
#include "..\Session\SessionManager.h"


std::atomic<bool> g_SessionThreadRun{ false };

std::thread g_SessionThread;


void SessionThread()
{
    auto lastTick = std::chrono::steady_clock::now();
    float syncTimer = 0.0f;
    const float syncInterval = 0.1f; // 100ms마다 위치 전송
    
     while (g_SessionThreadRun) {
        auto currentTick = std::chrono::steady_clock::now();
        // 실제 경과 시간(deltaTime) 계산
        float deltaTime = std::chrono::duration<float>(currentTick - lastTick).count();
        lastTick = currentTick;

        // 모든 세션의 이동 및 로직 업데이트
        GSessionManager.UpdateSssionMovement(deltaTime);


        //일 정 주기마다 위치 패킷 전송.
        // 2. 일정 주기마다만 위치 패킷 전송
        syncTimer += deltaTime;
        if (syncTimer >= syncInterval) {
            GSessionManager.BroadcastAllLocations(); // 이동 중인 유저들의 좌표를 모아서 전송
            syncTimer = 0.0f;
        }

        // CPU 점유율 폭주 방지 및 일정한 틱 레이트(예: 60FPS = 약 16ms) 유지
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }


}

void StartSessionThread()
{
    if (g_SessionThread.joinable()) {
        LOG("세션 스레드가 이미 실행 중입니다.\n");
        return;
    }

    g_SessionThreadRun = true;
    g_SessionThread = std::thread(SessionThread);
}

void CleanSessionThread()
{
    g_SessionThreadRun = false;

    if (g_SessionThread.joinable()) {
        g_SessionThread.join();
        //std::cout << "Monster Thread joined.\n";
    }

}

