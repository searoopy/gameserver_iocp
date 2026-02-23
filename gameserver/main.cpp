#include <iostream>


#include <WinSock2.h>
#include <MSWSock.h>
#include <thread>
#include <ws2tcpip.h>

#include <vector>



#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#ifdef _DEBUG
#define new new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#endif


#pragma comment(lib, "ws2_32.lib")


#include "Common.h"
#include "MemoryPool.h"
#include "Session\SessionManager.h"
#include "PROTOCOL\ProcessPacket.h"
#include "Threads\WorkerThread.h"
#include "Threads\MonitorSessions.h"



int main()
{


	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//_CrtSetBreakAlloc(342);

	//1. 윈속 초기화
	WSAData wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{

		std::cout << "winsock 초기화 실패\n";

		return 0;
	}

	GMemoryPool = new MemoryPool(MAX_SESSION*4); // 동접*4만큼 우표 미리 생성


	//2. iocp 핸들 생성.
	//시스템이 자동으로 스레드수 결정하도록 0을 전달.
	HANDLE hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hIOCP == NULL)
	{
		std::cout << "IOCP  핸들 생성 실패 \n";
		return 0;
	}


	std::cout << "IOCP 서버 준비 완료\n";


	//리슨 소켓 생성및 iocp등록-----
	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (listenSocket == INVALID_SOCKET)
	{
		std::cout << "소켓 생성 실패: " << WSAGetLastError() << "\n";
		return 0;
	}


	//2. 주소 설정 및 BIND
	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(9000);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);


	if (bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		std::cout << "Bind 실패\n";
		return 0;
	}


	//3. listen 시작.
	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		std::cout << "Listen 실패 \n";
		return 0;
	}


	//4. 리슨 소켓을 IOCP핸들에 등록
	if( CreateIoCompletionPort((HANDLE)listenSocket, hIOCP,0,0 ) == NULL )
	{
		std::cout << "소켓 iocp 등록 실패\n";
		return 0;
	}

	std::cout << "소켓이 IOCP에 성공적으로 등록되었습니다\n";

	LoadAcceptEx(listenSocket); // <--- 이게 RegisterAccept보다 먼저 와야 함!

	for (int i = 0; i < 5; i++) {
		RegisterAccept(hIOCP, listenSocket);
	}

	std::cout << "클라이언트 접속 대기 중..." << std::endl;



	// main 함수 안에서...

	std::thread monitorThread(MonitorSessions);
	monitorThread.detach();




	std::vector<std::thread> workerThreads;
	int threadCount = std::thread::hardware_concurrency() * 2;
	for (int i = 0; i < threadCount; ++i) {
		//std::thread(WorkerThread, hIOCP, listenSocket ).detach();
		workerThreads.emplace_back(WorkerThread, hIOCP, listenSocket);
	}






	//종료 방지용...
	getchar();





	//종료 로직 시작..
	closesocket(listenSocket);



	////스레드 종료 로직....
	for (int i = 0; i < threadCount; ++i) {
		// NULL overlapped를 보내서 스레드 종료 유도
		PostQueuedCompletionStatus(hIOCP, 0, 0, NULL);
	}

	// 스레드가 다 죽을 때까지 잠시 대기하거나 join을 사용하도록 구조 변경 추천
	//Sleep(1000);

	// 3. 스레드들이 정리될 시간을 잠시 줍니다. (또는 thread.join() 사용)
	//std::this_thread::sleep_for(std::chrono::milliseconds(500));
	for (auto& t : workerThreads)
	{
		if (t.joinable())
		{
			t.join(); //각 스레드가 while루프 빠져나와 리턴할때까지 여기서 멈춤....
		}
	}




	//정리...
	CloseHandle(hIOCP);
	WSACleanup();

	//메모리 해제
	if (GMemoryPool) {
		delete GMemoryPool; // 메모리 풀 반납
		GMemoryPool = nullptr;
	}


	return 0;

}



