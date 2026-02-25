#include "WorkerThread.h"
#include "..\Common.h"
#include "..\Session\SessionManager.h"
#include "..\PROTOCOL\ProcessPacket.h"
#include "..\PROTOCOL\Protocol.h"


#define MAX_BUFFER_SIZE 1024
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
        if (overlapped == NULL && completionKey == 0) break;

        OverlappedEx* ovEx = reinterpret_cast<OverlappedEx*>(overlapped);
        Session* session = reinterpret_cast<Session*>(completionKey);

        // 2. I/O 실패 처리 (소켓 끊김 등)
        if (!result) {
            if (session != nullptr) HandleDisconnect(session);
            if (ovEx != nullptr) GMemoryPool->Push(ovEx);
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

            Session* newSession = GSessionManager.Acquire();
            if (newSession == nullptr) {
                closesocket(ovEx->sessionSocket);
                GMemoryPool->Push(ovEx);
                break;
            }
            newSession->socket = ovEx->sessionSocket;
            CreateIoCompletionPort((HANDLE)newSession->socket, hIOCP, (ULONG_PTR)newSession, 0);

            // [수정] 첫 RECV 예약 (WSASend -> WSARecv로 변경)
            OverlappedEx* recvOv = GMemoryPool->Pop();
            memset(recvOv, 0, sizeof(OVERLAPPED)); // 헤더만 초기화
            recvOv->type = IO_TYPE::RECV;

            WSABUF wsaBuf;
            wsaBuf.buf = newSession->recvBuffer;
            wsaBuf.len = newSession->GetFreeSize();
            DWORD flags = 0;

            if (WSARecv(newSession->socket, &wsaBuf, 1, NULL, &flags, (LPOVERLAPPED)recvOv, NULL) == SOCKET_ERROR) {
                if (WSAGetLastError() != ERROR_IO_PENDING) {
                    GMemoryPool->Push(recvOv);
                    HandleDisconnect(newSession);
                }
            }
            GMemoryPool->Push(ovEx);
            break;
        }

        case IO_TYPE::RECV:
        {
            if (bytesTransferred == 0) {
                HandleDisconnect(session); // 주석 해제
                GMemoryPool->Push(ovEx);
                break;
            }

            session->writePos += bytesTransferred;

            while (true) {
                if (session->GetDataSize() < sizeof(PacketHeader)) break;
                PacketHeader* header = reinterpret_cast<PacketHeader*>(session->recvBuffer + session->readPos);

                if (header->size < sizeof(PacketHeader) || header->size > 1024 * 10) { // 매직 넘버 방어
                    HandleDisconnect(session);
                    GMemoryPool->Push(ovEx);
                    return;
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

            if (WSARecv(session->socket, &wsaBuf, 1, NULL, &flags, (LPOVERLAPPED)ovEx, NULL) == SOCKET_ERROR) {
                if (WSAGetLastError() != ERROR_IO_PENDING) {
                    HandleDisconnect(session);
                    GMemoryPool->Push(ovEx);
                }
            }
            break;
        }

        case IO_TYPE::SEND:
        {
            Session* session = reinterpret_cast<Session*>(completionKey);
            std::vector<OverlappedEx*> finishedList;
            WSABUF wsaBufs[WSA_BUFF_CNT];
            int bufCount = 0;
            OverlappedEx* firstTarget = nullptr;

            {
                std::lock_guard<std::mutex> lock(session->sendMutex);

                // 1. [중요] 지난번에 실제 '전송 요청'했던 개수만큼 큐에서 제거
                int lastPendingCount = session->sendPendingCount;
                for (int i = 0; i < lastPendingCount; ++i) {
                    if (!session->sendQueue.empty()) {
                        finishedList.push_back(session->sendQueue.front());
                        session->sendQueue.pop_front();
                    }
                }
                session->sendPendingCount = 0; // 초기화

                // 2. 새로 보낼 패킷들 모으기
                if (!session->sendQueue.empty()) {
                    for (auto* ov : session->sendQueue) {
                        if (bufCount >= WSA_BUFF_CNT) break;
                        wsaBufs[bufCount].buf = ov->buffer;
                        wsaBufs[bufCount].len = reinterpret_cast<PacketHeader*>(ov->buffer)->size;

                        if (bufCount == 0) firstTarget = ov;
                        bufCount++;
                    }
                    // 이번에 몇 개 보내는지 기록!
                    session->sendPendingCount = bufCount;
                }
                else {
                    session->isSending = false;
                }
            }

            // 락 밖에서 메모리 반납
            for (auto f : finishedList) GMemoryPool->Push(f);

            // 3. 전송
            if (firstTarget) {
                DWORD sentBytes = 0;
                if (WSASend(session->socket, wsaBufs, bufCount, &sentBytes, 0, (LPOVERLAPPED)firstTarget, NULL) == SOCKET_ERROR) {
                    if (WSAGetLastError() != ERROR_IO_PENDING) {
                        std::lock_guard<std::mutex> lock(session->sendMutex);
                        session->isSending = false;
                        session->sendPendingCount = 0;
                    }
                }
            }
            break;
        }//send...





        }
    }
}