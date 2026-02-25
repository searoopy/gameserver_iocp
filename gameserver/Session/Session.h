#pragma once

#include <WinSock2.h>
#include <mutex>

#include <list>
#include <string>
#include "..\Util\Util.h"
#include "..\Common.h"
#include "..\MemoryPool.h"
#include <random>


// 클라이언트 정보를 담는 세션
struct Session {
	SOCKET socket;
	char recvBuffer[SESSION_BUFFER_SIZE];

	std::atomic<bool> isFree;
	//bool isFree = true;
	int32_t sessionIdx = -1;

	// 여기에 나중에 OverlappedEx 객체들을 추가할 겁니다.

	uint64_t lastHeartbeatTick = 0;
	float x = 0.0f;
	float y = 0.0f;
	float yaw = 0.0f;
	int32_t userUid = -1;	//db상 고유번호
	std::wstring nickname;	//유저 닉네임
	bool isAuth = false;	//로그인 인증여부

	//send용 변수들.
	std::mutex sendMutex;
	std::deque<OverlappedEx*> sendQueue; // 보낼 데이터 담아 두는곳.
	bool isSending = false;
	int readPos;
	int writePos;
	int sendPendingCount = 0;


	void Reset()
	{
		userUid = -1;
		nickname.clear();
		isAuth = false;
		//isFree = true;
		readPos = 0;
		writePos = 0;
		lastHeartbeatTick = GetTickCount64();

		x = static_cast<float>(get_random_number(0, 375));
		y = static_cast<float>(get_random_number(0, 667)) ;

		yaw = 0.0f;




		// 큐에 남은 메모리 정리 (재사용 시 이전 유저의 패킷이 남아있으면 안 됨)
		std::lock_guard<std::mutex> lock(sendMutex);
		while (!sendQueue.empty()) {
			auto* ov = sendQueue.front();
			sendQueue.pop_front();
			GMemoryPool->Push(ov);
		}
		isSending = false; // 전송 상태도 초기화

		// 소켓 초기화 추가
		if (socket != INVALID_SOCKET) {
			// 이미 closesocket이 호출되었겠지만, 안전을 위해 보장
			socket = INVALID_SOCKET;
		}
	}


	//생성자
	Session() : socket(INVALID_SOCKET), readPos(0), writePos(0) {
		memset(recvBuffer, 0, sizeof(recvBuffer));

		Reset();

	}


	~Session()
	{
		// 소멸 시점에도 락을 걸어 다른 스레드의 접근을 차단합니다.
		std::lock_guard<std::mutex> lock(sendMutex);

		//세션 파괴시 큐에 남은 메모리 정리.
		while (!sendQueue.empty())
		{
			GMemoryPool->Push(sendQueue.front());
			sendQueue.pop_front();
		}
	}


	int GetDataSize() { return writePos - readPos; }
	int GetFreeSize() { return sizeof(recvBuffer) - writePos; }

	// [중요] 데이터를 앞으로 밀어버리는 타이밍 최적화
	// 무조건 밀지 않고, 공간이 부족할 때만 한 번씩 밀어줍니다.
	void CleanBuffer() {
		int dataSize = GetDataSize();
		if (dataSize <= 0) {
			readPos = writePos = 0;
		}
		else {
			// 남은 공간이 너무 적으면 앞으로 당김
			//if (GetFreeSize() < 1024) {
				if (readPos > 0) {
					memmove(recvBuffer, recvBuffer + readPos, dataSize);
					readPos = 0;
					writePos = dataSize;
				}
			//}
		}
	}


};

