#pragma once
#include <set>
#include <mutex>
#include "Common.h"
#include <list>
#include <string>
//#include <map>

// 클라이언트 정보를 담는 세션
struct Session {
	SOCKET socket;
	char recvBuffer[1024 * 10];


	// 여기에 나중에 OverlappedEx 객체들을 추가할 겁니다.

	uint64_t lastHeartbeatTick = 0;


	int32_t userUid = -1;	//db상 고유번호
	std::wstring nickname;	//유저 닉네임
	bool isAuth = false;	//로그인 인증여부
	void Reset()
	{
		userUid = -1;
		nickname.clear();
		isAuth = false;
	}


	//send용 변수들.
	std::mutex sendMutex;
	std::queue<OverlappedEx*> sendQueue; // 보낼 데이터 담아 두는곳.
	bool isSending = false;



	int readPos;
	int writePos;


	//생성자
	Session() : socket(INVALID_SOCKET), readPos(0), writePos(0) {
		memset(recvBuffer, 0, sizeof(recvBuffer));

		lastHeartbeatTick = GetTickCount64();

	}


	~Session()
	{
		// 소멸 시점에도 락을 걸어 다른 스레드의 접근을 차단합니다.
		std::lock_guard<std::mutex> lock(sendMutex);

		//세션 파괴시 큐에 남은 메모리 정리.
		while (!sendQueue.empty())
		{
			GMemoryPool->Push(sendQueue.front());
			sendQueue.pop();
		}
	}


	int GetDataSize() { return writePos - readPos; }
	int GetFreeSize() { return sizeof(recvBuffer) - writePos; }

	// [중요] 데이터를 앞으로 밀어버리는 타이밍 최적화
	// 무조건 밀지 않고, 공간이 부족할 때만 한 번씩 밀어줍니다.
	void CleanBuffer() {
		int dataSize = GetDataSize();
		if (dataSize == 0) {
			readPos = writePos = 0;
		}
		else {
			// 남은 공간이 너무 적으면 앞으로 당김
			if (GetFreeSize() < 1024) {
				memmove(recvBuffer, recvBuffer + readPos, dataSize);
				readPos = 0;
				writePos = dataSize;
			}
		}
	}


};





class SessionManager {
public:
    void Add(Session* session) {
        std::lock_guard<std::mutex> lock(m_mutex);
        //m_sessions.insert(session);
        m_sessions.emplace_back(session);
        std::cout << "세션 추가됨. 현재 접속자: " << m_sessions.size() << "명" << std::endl;
    }

    void Remove(Session* session) {
        std::lock_guard<std::mutex> lock(m_mutex);
        //m_sessions.erase(session);
        m_sessions.remove(session);
        std::cout << "세션 제거됨. 현재 접속자: " << m_sessions.size() << "명" << std::endl;
    }


	std::vector<Session*> GetSessionsCopy() {
		std::lock_guard<std::mutex> lock(m_mutex);
		return std::vector<Session*>(m_sessions.begin(), m_sessions.end());
	}

	std::list<Session*>& GetSessions() { return m_sessions; }
	std::mutex& GetMutex() { return m_mutex; }

    // 모든 접속자에게 패킷 전송 (채팅 등)
    void Broadcast(char* buffer, int len);
    void Broadcast(OverlappedEx* sendOv);


private:
    std::mutex m_mutex;
    std::list<Session*> m_sessions;
};



void SendPacket(Session* session, OverlappedEx* sendOv);
void HandleDisconnect(Session* session);


extern SessionManager GSessionManager;