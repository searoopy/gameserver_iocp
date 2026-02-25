#include "MonitorSessions.h"
#include <WinSock2.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include "..\Session\SessionManager.h"

void MonitorSessions() {
    while (true) {
        // 5초 대기
        std::this_thread::sleep_for(std::chrono::seconds(5));

        uint64_t currentTick = GetTickCount64();

        // 1. 스냅샷 찍기 (세션 매니저 내부 뮤텍스로 안전하게 복사본 확보)
        std::vector<Session*> activeSessions = GSessionManager.GetSessionsSnapshot();

        int authUserCount = 0;
        int timeoutCount = 0;

        // 2. 검사 루프
        for (Session* session : activeSessions) {
            // 이미 반납되었거나 정리 중인 세션은 스킵 (isFree 체크 필수)
            if (session->isFree.load() || session->socket == INVALID_SOCKET) continue;

            // 인증된 유저 카운트 (통계용)
            if (session->isAuth) authUserCount++;

            // 3. 타임아웃 체크 (30초 이상 무응답)
            if (currentTick - session->lastHeartbeatTick > 30000) {
                timeoutCount++;
                std::cout << "[Monitor] 타임아웃 감지! ID: " << session->sessionIdx
                    << " (Nickname: " << (session->nickname.empty() ? L"Unknown" : session->nickname.c_str()) << ")" << std::endl;

                // 세션 종료 처리 (이미 atomic 처리가 되어있으므로 안전하게 호출 가능)
                HandleDisconnect(session);
            }
        }



        // 4. 통계 출력 및 저장
        int totalConnected = GSessionManager.GetConnectSessionCnt();

        // [콘솔 출력]
        std::cout << "\n--- [Server Monitor] " << totalConnected << " Users Connected ---" << std::endl;
        if (timeoutCount > 0) std::cout << " > Disconnected by Timeout: " << timeoutCount << std::endl;
        std::cout << " > Active Players (In-Game): " << authUserCount << std::endl;
        std::cout << "-------------------------------------------\n" << std::endl;


        /*
        // [파일 기록]
        std::ofstream logFile("server_status.log", std::ios::app);
        if (logFile.is_open()) {
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            struct tm timeinfo;
            localtime_s(&timeinfo, &now);

            logFile << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S")
                << " | Conn: " << totalConnected
                << " | Auth: " << authUserCount
                << " | TO: " << timeoutCount << "\n";
            logFile.close();
        }
        */
    }
}