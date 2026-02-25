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

            m_pSessions[i].sessionIdx = i;

            m_pSessions[i].isFree = true;

            
        }

    }

    ~SessionManager()
    {
        delete[] m_pSessions;
    }

    void AddActiveSession(Session* session) {
        // 1. 전용 락 사용 (m_mutex 대신 m_activeMutex)
        std::lock_guard<std::mutex> lock(m_activeMutex);

        // 2. 검색(std::find) 과감히 제거
        // 세션의 상태 플래그 등을 믿고 바로 밀어넣습니다. 
        // 정 중복이 걱정된다면 session->isRegistered 같은 bool 변수를 활용하세요.
        m_activeSessions.push_back(session);
    }


    Session* Acquire() {
        int index = -1;
        {
            // 1. 할당 락은 아주 짧게!
            std::lock_guard<std::mutex> lock(m_poolMutex);
            if (m_freeStack.empty()) return nullptr;
            index = m_freeStack.top();
            m_freeStack.pop();
        }

        Session* session = &m_pSessions[index];
        session->Reset();
        session->isFree.store(false);

        {
            // 2. 활동 목록 추가 락 분리
            std::lock_guard<std::mutex> lock(m_activeMutex);
            m_activeSessions.push_back(session);
        }

        m_connectedCount.fetch_add(1);

        return session;
    }

    void Release(Session* session)
    {
        {
            // 3. 활동 목록 제거 (가장 무거운 작업)
            std::lock_guard<std::mutex> lock(m_activeMutex);
            // vector의 erase 성능을 위해 swap-back 기법 사용 추천
            auto it = std::find(m_activeSessions.begin(), m_activeSessions.end(), session);
            if (it != m_activeSessions.end()) {
                // 맨 뒤 원소와 바꾼 후 pop_back하면 O(1)에 가깝게 제거 가능
                std::iter_swap(it, m_activeSessions.end() - 1);
                m_activeSessions.pop_back();
            }
        }


        // 큐에 남은 패킷 정리 (메모리 누수 방지)
        {
            std::lock_guard<std::mutex> lock(session->sendMutex);
            while (!session->sendQueue.empty()) {
                OverlappedEx* ov = session->sendQueue.front();
                session->sendQueue.pop_front();
                GMemoryPool->Push(ov);
            }
            session->isSending = false;
        }

        session->isFree.store(true);

        {
            // 4. 할당 락 분리
            std::lock_guard<std::mutex> lock(m_poolMutex);
            m_freeStack.push(session->sessionIdx);
        }

        m_connectedCount.fetch_sub(1);

    }
    

    int GetConnectSessionCnt() {
        // 락을 걸지 않아도 원자적(Atomic)으로 안전하게 값을 읽어옵니다.
        return m_connectedCount.load();
    }

    // 데드락 방지를 위한 스냅샷 기능
    std::vector<Session*> GetSessionsSnapshot() {
        std::lock_guard<std::mutex> lock(m_activeMutex);
        return m_activeSessions; // 복사본 반환
    }

    // 모든 접속자에게 패킷 전송 (채팅 등)
    void Broadcast(OverlappedEx* sendOv);
    void BroadcastNewUser(Session* newUser);
    void BroadcastLeaveUser(int32_t leavingUser);

private:
    std::mutex m_poolMutex;      // 세션 할당/반납용 락
    std::mutex m_activeMutex;    // 활동 목록 관리용 락

    std::atomic<int> m_connectedCount{ 0 };

    Session* m_pSessions; 
    std::stack<int> m_freeStack;       // 비어있는 인덱스 관리
    std::vector<Session*> m_activeSessions; // 현재 접속 중인 세션들 (BroadCast용)
};

void SendPacket(Session* session, OverlappedEx* sendOv);
void HandleDisconnect(Session* session);


extern SessionManager GSessionManager;