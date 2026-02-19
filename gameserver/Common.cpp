#include "Common.h"


LPFN_ACCEPTEX lpfnAcceptEx = NULL; // 함수 포인터 저장용

void LoadAcceptEx(SOCKET listenSocket) {
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	DWORD bytes;

	// WSAIoctl을 사용해 AcceptEx 함수의 주소를 런타임에 가져옵니다.
	WSAIoctl(listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx, sizeof(GuidAcceptEx),
		&lpfnAcceptEx, sizeof(lpfnAcceptEx),
		&bytes, NULL, NULL);
}


bool RegisterAccept(HANDLE hIOCP, SOCKET listenSocket) {
	// 1. 미리 소켓을 생성
	SOCKET clientSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	// 2. 컨텍스트 할당 (힙 영역)
	OverlappedEx* acceptContext = GMemoryPool->Pop();//new OverlappedEx();
	memset(acceptContext, 0, sizeof(OverlappedEx));
	acceptContext->type = IO_TYPE::ACCEPT;
	acceptContext->sessionSocket = clientSocket;

	DWORD bytesReceived = 0;
	// 3. AcceptEx 호출
	// 주소 버퍼는 최소 (sizeof(SOCKADDR_IN) + 16) * 2 크기여야 합니다.
	if (lpfnAcceptEx(listenSocket, clientSocket, acceptContext->buffer, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		&bytesReceived, (LPOVERLAPPED)acceptContext) == FALSE) {
		if (WSAGetLastError() != ERROR_IO_PENDING) {
			//delete acceptContext;
			GMemoryPool->Push(acceptContext);
			return false;
		}
	}
	return true;
}