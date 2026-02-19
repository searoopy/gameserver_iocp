#include "WorkerThread.h"
#include "..\Common.h"
#include "..\SessionManager.h"
#include "..\PROTOCOL\ProcessPacket.h"

void WorkerThread(HANDLE hIOCP, SOCKET listenSocket)
{
	DWORD bytesTransferred;
	ULONG_PTR completionKey;
	LPOVERLAPPED overlapped = nullptr;

	while (true)
	{
		//여기서 신호가 올때까지 스레드는 잠듭니다( cpu 점유율 0%)
		BOOL result = GetQueuedCompletionStatus(
			hIOCP,
			&bytesTransferred,				//전송된 바이트 수
			&completionKey,					//우리가 등록한 키 (세션 정보 등)
			&overlapped,					//작업이 안료된 오버렙 구조체 주소
			INFINITE
		);

		//종료 신호 .(가장 먼저 체크)
		if (overlapped == NULL && completionKey == 0) {


			//??
			///클라 종료?
			std::cout << "클라이언트 오버랩 null 키 == 0 !\n";

			//클라 종료됨..주석테스트.

			break;
		}


		if (overlapped != nullptr)
		{
			OverlappedEx* ovEx = reinterpret_cast<OverlappedEx*>(overlapped);

			if (!result) // 소켓이 닫히는 등의 이유로 실패한 경우
			{
				DWORD error = GetLastError();
				// 소켓이 강제로 닫히거나 연결이 끊긴 경우 여기로 들어옵니다.
				Session* session = reinterpret_cast<Session*>(completionKey);
				std::cout << "비정상 종료 감지: 에러코드 " << error << "\n";

				// 여기서 세션 정리 로직(HandleDisconnect 등)을 반드시 호출해야 합니다.
				HandleDisconnect(session);



				GMemoryPool->Push(ovEx);
				continue;
			}

			switch (ovEx->type)
			{
			case IO_TYPE::ACCEPT:
			{
				std::cout << "새로운 클라이언트 접속 완료!" << std::endl;

				// 1. 접속된 소켓을 IOCP에 등록
				// 2. 해당 소켓으로 RECV(수신) 예약
				// 3. 다시 다음 클라이언트를 위해 AcceptEx 예약
				// 1. 소켓 속성 업데이트
				setsockopt(ovEx->sessionSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
					(char*)&listenSocket, sizeof(SOCKET));

				//[ 세션 객체 생성 및 등록 ] 
				Session* newSession = new Session();
				newSession->socket = ovEx->sessionSocket;
				GSessionManager.Add(newSession);

				// 2. 새 클라이언트를 IOCP에 등록
				CreateIoCompletionPort((HANDLE)ovEx->sessionSocket, hIOCP, (ULONG_PTR)newSession, 0);

				// 3. 수신(RECV) 예약
				// ... WSARecv 호출 코드 ...
				// [C] 첫 번째 데이터 수신(RECV) 예약
				// (여기서 새로운 RECV용 OverlappedEx를 만들어 넘깁니다.)
				OverlappedEx* recvOv = GMemoryPool->Pop();
				memset(recvOv, 0, sizeof(OverlappedEx));
				recvOv->type = IO_TYPE::RECV;
				recvOv->sessionSocket = ovEx->sessionSocket;

				WSABUF wsaBuf;
				wsaBuf.buf = newSession->recvBuffer;
				wsaBuf.len = newSession->GetFreeSize();

				DWORD flags = 0;
				WSARecv(newSession->socket, &wsaBuf, 1, NULL, &flags, (LPOVERLAPPED)recvOv, NULL);

				// 4. 중요: 다음 접속자를 위해 다시 예약!
				RegisterAccept(hIOCP, listenSocket);


				// 사용이 끝난 ACCEPT용 컨텍스트는 해제하거나 풀(Pool)에 반납
				GMemoryPool->Push(ovEx);
				break;

			}
			case IO_TYPE::RECV:
			{

				// CompletionKey를 세션 포인터로 복원
				Session* session = reinterpret_cast<Session*>(completionKey);



				if (bytesTransferred == 0) {
					// 중요: 0바이트 전송은 클라이언트의 정상 종료를 의미합니다.
					std::cout << "클라이언트 접속 종료!\n";
					GSessionManager.Remove(session);
					closesocket(session->socket);
					//delete ovEx;

					delete session;

					GMemoryPool->Push(ovEx);
					break;
				}


				// 클라이언트가 보낸 데이터가 도착함!
				std::cout << "데이터 수신: " << bytesTransferred << " bytes\n";



				//1. 방금 받은 데이터를 세션의 누적 버퍼에 복사
				//memcpy(session->recvBuffer + session->recvWriteSize,
				//	ovEx->buffer, bytesTransferred);
				//1. 커서 이동.
				session->writePos += bytesTransferred;


				//패킷 헤더 사이즈만큼은 와야한다
				while (true)
				{
					// 헤더만큼은 들어있는가?
					if (session->GetDataSize() < sizeof(PacketHeader)) break;

					// 헤더를 살짝 훔쳐보기 (peek)
					PacketHeader* header = reinterpret_cast<PacketHeader*>(session->recvBuffer + session->readPos);

					// 전체 패킷이 다 도착했는가?
					if (session->GetDataSize() < header->size) break;

					// [패킷 완성!] 이제 이 데이터를 처리합니다.
					// 여기서 header->id에 따라 에코를 하든, 로그를 찍든 처리 로직을 실행합니다.
					ProcessPacket(session, header);


					// 3. 읽은 만큼 커서 이동
					session->readPos += header->size;

				}//end of while.


				//4. 버퍼 정리 (효율적인 타이밍만 수행)
				session->CleanBuffer();


				////////////////////////////////////////////////////////////////////////////////////////////
				// --- [추가] Echo: 받은 데이터를 그대로 다시 보내기 ---
				///todo: 에코 메시지...?
				// ----------------------------------------------


				// 다시 수신 예약 (기존 ovEx 재사용)
				///memset(ovEx->buffer, 0, 1024); // 버퍼 초기화
				WSABUF wsaBuf;
				DWORD flags = 0;
				wsaBuf.buf = session->recvBuffer + session->writePos;
				wsaBuf.len = session->GetFreeSize();

				//WSARecv(session->socket, &wsaBuf, 1, NULL, &flags, (LPOVERLAPPED)ovEx, NULL);
				if (WSARecv(session->socket, &wsaBuf, 1, NULL, &flags, (LPOVERLAPPED)ovEx, NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != ERROR_IO_PENDING) {
						// 에러 처리
					}
				}

				break;
			}

			case IO_TYPE::SEND:
			{


				Session* session = reinterpret_cast<Session*>(completionKey);
				// 방금 완료된 ovEx는 이미 큐의 front에 있습니다.

				std::lock_guard<std::mutex> lock(session->sendMutex);

				// 1. 다 보낸 녀석은 풀에 반납
				OverlappedEx* finished = session->sendQueue.front();
				session->sendQueue.pop();
				GMemoryPool->Push(finished);


				// 2. 큐에 더 보낼 게 남았나 확인
				if (session->sendQueue.empty()) {
					session->isSending = false;
				}
				else {
					// 남은 게 있다면 다음 녀석 전송 시작
					OverlappedEx* next = session->sendQueue.front();
					WSABUF wsaBuf;
					wsaBuf.buf = next->buffer;
					wsaBuf.len = reinterpret_cast<PacketHeader*>(next->buffer)->size;

					DWORD sentBytes = 0;
					WSASend(session->socket, &wsaBuf, 1, &sentBytes, 0, (LPOVERLAPPED)next, NULL);
					// 여기서 isSending은 여전히 true 유지
				}
				break;



				// 보낸 후에는 전송용으로 만든 메모리 해제
				//delete ovEx;
				//GMemoryPool->Push(ovEx);
				break;
			}


			}
		}
		else if (!result)
		{
			DWORD error = GetLastError();
			if (overlapped != nullptr) {
				// 비정상 종료 상황 (예: 세션 에러)
				Session* session = reinterpret_cast<Session*>(completionKey);
				std::cout << "비정상 접속 종료! 에러 코드: " << error << "\n";
				HandleDisconnect(session);
			}
			continue;

		}


		//여기서 비동기 작업 결과 로직 처리
		//수신된 패킷 분석 ,. DB저장, 다른유저에게 전달등.
		std::cout << bytesTransferred << "바이트 작업 완료 확인 \n";

	}
}
