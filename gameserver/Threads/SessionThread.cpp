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
    auto targetTickTime = std::chrono::milliseconds(16); // 약 60 FPS

    while (g_SessionThreadRun) {
        auto currentTick = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTick - lastTick).count();
        lastTick = currentTick;

        // 1. 물리/이동 업데이트 (매 프레임)
        g_pSessionManager->UpdateSssionMovement(deltaTime);

        // 2. 패킷 전송 (여기서는 매 프레임 전송으로 가정, 필요시 타이머 추가)
        auto targets = g_pSessionManager->GetSessionsSnapshot();
        for (Session* s : targets) {
            if (s->bPosChanged) {
                g_pSessionManager->BroadcastMoveToNearby(s);
                s->bPosChanged = false;
            }
        }

        // 3. 가변 Sleep (중요!)
        auto frameEndTick = std::chrono::steady_clock::now();
        auto processTime = frameEndTick - currentTick; // 실제 로직 수행 시간

        if (processTime < targetTickTime) {
            // 남은 시간만큼만 정확히 쉰다
            std::this_thread::sleep_for(targetTickTime - processTime);
        }
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

