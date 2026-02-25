#pragma once

#include <iostream>


#include <WinSock2.h>
#include <MSWSock.h>
#include <thread>
#include <ws2tcpip.h>
#include <queue>


//#include "PROTOCOL\Protocol.h"
//#include "MemoryPool.h"
//#include "Session/SessionManager.h"
//#include "PROTOCOL\PacketHandler.h"



//현대 운영체제(Windows)의 메모리 관리 최소 단위인 Page Size가 보통 **$4KB$**입니다.
#define MAX_BUFFER_SIZE 1024*4
//TCP의 수신 윈도우(Receive Window) 기본 최대치가
// $64KB$ 근처이기 때문에,
// 창고(Session Buffer) 크기를 이 정도로 맞춰주면 네트워크 카드가 보내주는 데이터를 병목 현상 없이 그대로 받아낼 수 있습니다.
#define SESSION_BUFFER_SIZE 1024 * 64


#define WSA_BUFF_CNT  32


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
	char buffer[MAX_BUFFER_SIZE];       // 데이터 수신용 버퍼

	OverlappedEx* next = nullptr; // lock -free stack을 위한 링크.
};



extern LPFN_ACCEPTEX lpfnAcceptEx ; // 함수 포인터 저장용
void LoadAcceptEx(SOCKET listenSocket);
bool RegisterAccept(HANDLE hIOCP, SOCKET listenSocket);
void OptimizeSocketBuffer(SOCKET socket);
