#include "MonitorSessions.h"
#include <WinSock2.h>
#include <chrono>
#include "..\SessionManager.h"

void MonitorSessions() {
    while (true) {
        // 5초마다 검사 (너무 자주 하면 CPU 낭비)
        std::this_thread::sleep_for(std::chrono::seconds(5));

        uint64_t currentTick = GetTickCount64();

        // 세션 매니저 내부에서 뮤텍스 락을 걸고 리스트를 가져와야 안전함
        // 예: lock_guard<mutex> lock(GSessionManager.GetMutex());
        std::lock_guard<std::mutex> lock(GSessionManager.GetMutex());

        auto& sessions = GSessionManager.GetSessions();

        for (auto it = sessions.begin(); it != sessions.end(); ) {
            Session* session = *it;

            // 30초 동안 하트비트가 없으면 끊긴 것으로 간주
            if (currentTick - session->lastHeartbeatTick > 30000) {
                std::cout << "[Monitor] 타임아웃 발생! 소켓 강제 종료. ID: " << session->socket << std::endl;

                // 소켓을 닫으면 WorkerThread의 GetQueuedCompletionStatus가 
                // FALSE를 반환하거나 bytesTransferred 0을 받아 정리 로직이 돌아감
                closesocket(session->socket);
                session->socket = INVALID_SOCKET;
                // 리스트에서 제거 및 메모리 해제
                it = sessions.erase(it);

               
            }
            else {
                ++it;
            }
        }
    }
}