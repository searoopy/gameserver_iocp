#pragma once
#include <mutex>
#include "..\Common.h"
#include <list>
#include <string>
#include <stack>
#include "Session.h"
//#include <map>


const int MAX_SESSION = 5000; //최대 동접자수.

class SessionManager {
public:


    SessionManager() {
        m_pSessions = new Session[MAX_SESSION];
        for (int i = 0; i < MAX_SESSION; ++i)
        {
            m_freeStack.push(i);

            m_pSessions[i].sessionID = i;

            m_pSessions[i].isFree = true;

            
        }

    }

    ~SessionManager()
    {
        delete[] m_pSessions;
    }

    void AddActiveSession(Session* session) {
        std::lock_guard<std::mutex> lock(m_mutex);

        // 1. 중복 등록 방지 (방어 코드)
        // 이미 리스트에 있다면 다시 넣지 않습니다.
        auto it = std::find(m_activeSessions.begin(), m_activeSessions.end(), session);
        if (it != m_activeSessions.end()) {
            return;
        }

        // 2. 활성 목록에 추가
        m_activeSessions.push_back(session);

        // 3. (선택 사항) 로그 출력
        // std::cout << "[Manager] 세션 활성화 완료: ID " << session->sessionID << std::endl;
    }

    // 새로운 클라이언트가 접속했을 때 빈 세션 하나를 할당해줍니다.
    Session* Acquire() {
        std::lock_guard<std::mutex> lock(m_mutex);
        // 1. 빈 방이 있는지 확인
        if (m_freeStack.empty()) {
            std::cout << "접속 제한: 서버가 꽉 찼습니다!" << std::endl;
            return nullptr;
        }

     

        int index = m_freeStack.top();
        m_freeStack.pop();

        int connectPlayer = MAX_SESSION - m_freeStack.size();
        std::cout << "접속자수: " << connectPlayer << "\n";

        Session* session = &m_pSessions[index];
        session->Reset(); // 세션 정보 초기화 (중요)
        session->isFree = false;

        // 현재 활동 중인 세션 목록에 추가   --> ENTER에서 활동중 세션 목록 추가로 옴김.
        //m_activeSessions.push_back(session);


        return session;
    }

    // 접속이 끊긴 세션을 풀로 반납합니다.
    void Release(Session* session) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (session->isFree) return;

    
        session->isFree = true;

        // 활동 목록에서 제거 (주의: vector의 erase는 느릴 수 있으나 스냅샷 용으로 유지)
        auto it = std::find(m_activeSessions.begin(), m_activeSessions.end(), session);
        if (it != m_activeSessions.end()) {
            m_activeSessions.erase(it);
        }

        m_freeStack.push(session->sessionID);

        int connectPlayer = MAX_SESSION - m_freeStack.size();
        std::cout << "접속자수: " << connectPlayer << "\n";

    }


    // 데드락 방지를 위한 스냅샷 기능
    std::vector<Session*> GetSessionsSnapshot() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_activeSessions; // 복사본 반환
    }


	std::mutex& GetMutex() { return m_mutex; }

    // 모든 접속자에게 패킷 전송 (채팅 등)
    void Broadcast(char* buffer, int len);
    void Broadcast(OverlappedEx* sendOv);
    void BroadcastNewUser(Session* newUser);


private:
    std::mutex m_mutex;

    Session* m_pSessions; 
    std::stack<int> m_freeStack;       // 비어있는 인덱스 관리
    std::vector<Session*> m_activeSessions; // 현재 접속 중인 세션들 (BroadCast용)
};

void SendPacket(Session* session, OverlappedEx* sendOv);
void HandleDisconnect(Session* session);


extern SessionManager GSessionManager;