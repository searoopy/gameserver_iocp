#include "MonitorSessions.h"
#include <WinSock2.h>
#include <chrono>
#include "..\Session\SessionManager.h"

void MonitorSessions() {
    while (true) {
        // 5초마다 검사 (너무 자주 하면 CPU 낭비)
        std::this_thread::sleep_for(std::chrono::seconds(5));

        uint64_t currentTick = GetTickCount64();

        // 세션 매니저 내부에서 뮤텍스 락을 걸고 리스트를 가져와야 안전함
        // 예: lock_guard<mutex> lock(GSessionManager.GetMutex());
       // std::lock_guard<std::mutex> lock(GSessionManager.GetMutex());

        // 1. 스냅샷 찍기 (락 타임 최소화)
        std::vector<Session*> activeSessions = GSessionManager.GetSessionsSnapshot();
        
        // 2. 락 밖에서 느긋하게 검사
        for (Session* session : activeSessions) {
            // 이미 나간 세션이면 패스
            if (session->isFree || session->socket == INVALID_SOCKET) continue;

            // 30초 타임아웃 체크
            if (currentTick - session->lastHeartbeatTick > 30000) {
                std::cout << "[Monitor] 타임아웃! ID: " << session->sessionID << std::endl;

                // 3. 강제 종료 수행
                // 여기서 closesocket을 하면 WorkerThread에서 에러를 감지하고 
                // 해당 스레드가 안전하게 Release(session)을 호출하게 됩니다.
                // 직접 여기서 리스트를 지우는 것보다 소켓만 닫는 게 안전합니다.
                closesocket(session->socket);
                session->socket = INVALID_SOCKET;
            }
        }
    }
}