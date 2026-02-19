#pragma once

#include <iostream>


#include <WinSock2.h>
#include <MSWSock.h>
#include <thread>
#include <ws2tcpip.h>
#include <queue>


#include "PROTOCOL\Protocol.h"
#include "MemoryPool.h"
#include "SessionManager.h"
#include "PROTOCOL\PacketHandler.h"


// 비동기 작업의 종류를 구분하기 위한 열거형
enum class IO_TYPE {
	ACCEPT,
	RECV,
	SEND
};

// Overlapped 구조체를 확장하여 커스텀 데이터 보관
struct OverlappedEx {
	OVERLAPPED overlapped; // 반드시 첫 번째 멤버여야 함
	IO_TYPE type;          // 작업 종류
	SOCKET sessionSocket;    // AcceptEx 시 새로 생성한 소켓 보관용
	char buffer[128];       // 데이터 수신용 버퍼
};



extern LPFN_ACCEPTEX lpfnAcceptEx ; // 함수 포인터 저장용
void LoadAcceptEx(SOCKET listenSocket);
bool RegisterAccept(HANDLE hIOCP, SOCKET listenSocket);

