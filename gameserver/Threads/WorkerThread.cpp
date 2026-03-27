#include "WorkerThread.h"
#include "..\Common.h"
#include "..\Session\SessionManager.h"
#include "..\PROTOCOL\ProcessPacket.h"
#include "..\PROTOCOL\Protocol.h"


void WorkerThread(HANDLE hIOCP, SOCKET listenSocket)
{
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    LPOVERLAPPED overlapped = nullptr;

    while (true)
    {
       
        BOOL result = GetQueuedCompletionStatus(
            hIOCP, &bytesTransferred, &completionKey, &overlapped, INFINITE
        );

        // 1. 종료 신호 처리
        if (overlapped == NULL) {
            // 종료 신호이거나 비정상적인 상황
            if (completionKey == 0) break;
            continue;
        }

        // [핵심 수정] overlapped 포인터로부터 OverlappedEx 구조체의 시작 주소를 얻어옴
        // 구조체 타입, 멤버 이름 순서로 넣어줍니다.
        OverlappedEx* ovEx = CONTAINING_RECORD(overlapped, OverlappedEx, overlapped);
       // OverlappedEx* ovEx = reinterpret_cast<OverlappedEx*>(overlapped);

        Session* session = nullptr;

        if (completionKey != LISTEN_KEY && completionKey != 0) {
            session = reinterpret_cast<Session*>(completionKey);
        }

        // 2. I/O 실패 처리 (소켓 끊김 등)
        if (!result) {
            if (session != nullptr) g_pSessionManager->HandleDisconnect(session);
            if (ovEx != nullptr) GMemoryPool->Push(ovEx);
            continue;
        }

        // 2. 리슨 소켓 이벤트인데 SEND/RECV가 왔다면 무시 (방어 코드)
        if (completionKey == LISTEN_KEY && ovEx->type != IO_TYPE::ACCEPT) {
           
            printf("!!! Critical: Logic Error. ListenKey with Type %d\n", ovEx->type);
            // GMemoryPool->Push(ovEx); // 이미 오염된 주소일 수 있으므로 주의
            continue;
        }

        if (session == nullptr && ovEx->type != IO_TYPE::ACCEPT) {
            // 세션이 없는데 SEND/RECV가 왔다면 로직 오류나 비정상 소켓입니다.
            if(ovEx!= nullptr)
                GMemoryPool->Push(ovEx);
            continue;
        }



        switch (ovEx->type)
        {
        case IO_TYPE::ACCEPT:
        {
            // 다음 Accept 미리 예약
            RegisterAccept(hIOCP, listenSocket);

            setsockopt(ovEx->sessionSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&listenSocket, sizeof(SOCKET));
            OptimizeSocketBuffer(ovEx->sessionSocket);

            Session* newSession = g_pSessionManager->Acquire();
            if (newSession == nullptr) {
                closesocket(ovEx->sessionSocket);
                GMemoryPool->Push(ovEx);
                break;
            }
            newSession->socket = ovEx->sessionSocket;
            if (CreateIoCompletionPort((HANDLE)newSession->socket, hIOCP, (ULONG_PTR)newSession, 0) == NULL) {
                // 등록 실패 처리
                g_pSessionManager->HandleDisconnect(newSession);
                GMemoryPool->Push(ovEx);
                break;
            }

            // [수정] 첫 RECV 예약 (WSASend -> WSARecv로 변경)
            OverlappedEx* recvOv = GMemoryPool->Pop();
            recvOv->Init();
            //memset(&recvOv->overlapped, 0, sizeof(OVERLAPPED));
            //memset(recvOv, 0, sizeof(OVERLAPPED)); // 헤더만 초기화
            recvOv->type = IO_TYPE::RECV;

            WSABUF wsaBuf;
            wsaBuf.buf = newSession->recvBuffer;
            wsaBuf.len = newSession->GetFreeSize();
            DWORD flags = 0;

            if (WSARecv(newSession->socket, &wsaBuf, 1, NULL, &flags, (LPOVERLAPPED)&recvOv->overlapped, NULL) == SOCKET_ERROR) {
                if (WSAGetLastError() != ERROR_IO_PENDING) {
                    GMemoryPool->Push(recvOv);
                    g_pSessionManager->HandleDisconnect(newSession);
                }
            }

            if (ovEx != nullptr)
                GMemoryPool->Push(ovEx);
            break;
        }

        case IO_TYPE::RECV:
        {
            if (bytesTransferred == 0) {
                g_pSessionManager->HandleDisconnect(session); // 주석 해제
                if(ovEx != nullptr)
                    GMemoryPool->Push(ovEx);
                break;
            }

            session->writePos += bytesTransferred;

            while (true) {
                if (session->GetDataSize() < sizeof(PacketHeader)) break;
                PacketHeader* header = reinterpret_cast<PacketHeader*>(session->recvBuffer + session->readPos);

                if (header->size < sizeof(PacketHeader) || header->size > 1024 * 10) { // 매직 넘버 방어
                    g_pSessionManager->HandleDisconnect(session);
                    GMemoryPool->Push(ovEx);
                    break; // return;
                }

                if (session->GetDataSize() < header->size) break;
                ProcessPacket(session, header);
                session->readPos += header->size;
            }

            session->CleanBuffer();

            WSABUF wsaBuf;
            wsaBuf.buf = session->recvBuffer + session->writePos;
            wsaBuf.len = session->GetFreeSize();
            DWORD flags = 0;

            if (WSARecv(session->socket, &wsaBuf, 1, NULL, &flags, (LPOVERLAPPED)&ovEx->overlapped, NULL) == SOCKET_ERROR) {
                if (WSAGetLastError() != ERROR_IO_PENDING) {
                    g_pSessionManager->HandleDisconnect(session);

                    if (ovEx != nullptr)
                        GMemoryPool->Push(ovEx);
                }
            }
            break;
        }

        case IO_TYPE::SEND:
        {
           
            Session* session = reinterpret_cast<Session*>(completionKey);

            if (!session) {
                if (ovEx) GMemoryPool->Push(ovEx);
                break;
            }

            if (session->socket == INVALID_SOCKET || session->isFree.load()) {
                // 전송 완료된 것들 메모리 반납 로직만 최소한으로 수행하거나 스킵
                break;
            }

           // Session* session = reinterpret_cast<Session*>(completionKey);
            std::vector<OverlappedEx*> toRelease;
            toRelease.reserve(WSA_BUFF_CNT);
            // 1. 다음 전송을 위한 준비물
            WSABUF wsaBufs[WSA_BUFF_CNT];
            int bufCount = 0;
            OverlappedEx* firstTarget = nullptr;
            bool initiateNextSend = false;


            {
                std::lock_guard<std::mutex> lock(session->sendMutex);


                // 세션이 이미 종료되었거나 큐가 비어있다면 무시 (중요!)
                if (session->isFree.load() || session->sendQueue.empty()) {
                    session->sendPendingCount = 0;
                    session->isSending = false;
                    // 여기서 return 하지 말고 밖에서 toRelease 정리만 하고 끝내야 함
                }
                else
                {

                    // --- [A] 방금 완료된 패킷들 정리 ---
                    int completedCount = session->sendPendingCount;
                    for (int i = 0; i < completedCount; ++i) {
                        if (session->sendQueue.empty()) break;
                        OverlappedEx* completed = session->sendQueue.front();
                        session->sendQueue.pop_front();

                        if (completed->refCount.fetch_sub(1) == 1) {
                            toRelease.push_back(completed);
                        }
                    }
                    session->sendPendingCount = 0;

                    // --- [B] 남은 패킷이 있다면 다음 전송 시작 (핵심) ---
                    if (session->sendQueue.empty()) {
                        session->isSending = false;
                    }
                    else {
                        // 아직 보낼 게 남았다면 다시 Gather
                        initiateNextSend = true;

                        auto it = session->sendQueue.begin();
                        for (; it != session->sendQueue.end() && bufCount < WSA_BUFF_CNT; ++it) {
                            OverlappedEx* ov = *it;
                            wsaBufs[bufCount].buf = ov->buffer;
                            wsaBufs[bufCount].len = reinterpret_cast<PacketHeader*>(ov->buffer)->size;

                            if (bufCount == 0) firstTarget = ov;
                            bufCount++;
                        }
                        session->sendPendingCount = bufCount;
                    }
                }
            }

            // --- [C] 락 밖에서 메모리 반납 및 다음 전송 호출 ---
            for (auto* ov : toRelease)
            {
                if (ov != nullptr)
                   GMemoryPool->Push(ov);
            }

            if (initiateNextSend && firstTarget) {
                SOCKET s = session->socket;
                if (s != INVALID_SOCKET)
                {
                    DWORD sentBytes = 0;
                    if (WSASend(session->socket, wsaBufs, bufCount, &sentBytes, 0, (LPOVERLAPPED)&firstTarget->overlapped, NULL) == SOCKET_ERROR)
                    {
                        if (WSAGetLastError() != ERROR_IO_PENDING)
                        {
                            // 전송 실패 시 처리
                            std::lock_guard<std::mutex> lock(session->sendMutex);
                            session->isSending = false;
                            session->sendPendingCount = 0;
                        }
                    }
                }
            }
            break;





        }//send...





        }
    }
}