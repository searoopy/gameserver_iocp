#include "SessionManager.h"
#include "Session.h"
#include "..\Common.h"
#include "..\PROTOCOL\Protocol.h"

SessionManager GSessionManager;


void SessionManager::Broadcast(OverlappedEx* sendOv) {
    std::vector<Session*> activeSessions = GetSessionsSnapshot();

    for (Session* session : activeSessions) {
        // 이미 종료 처리 중인 세션은 건너뜁니다.
        if (session->isFree.load()) continue;

        // 새로운 OverlappedEx 할당
        OverlappedEx* copyOv = GMemoryPool->Pop();

        // [중요] 전체 memcpy 대신 필요한 데이터만 복사
        // OVERLAPPED 구조체 영역은 0으로 초기화되어야 합니다.
        memset(copyOv, 0, sizeof(OverlappedEx));

        copyOv->type = sendOv->type;
        copyOv->sessionSocket = session->socket; // 대상 세션의 소켓으로 설정

        // 패킷 헤더를 통해 실제 복사할 크기 계산 (전체 버퍼 복사보다 효율적)
        PacketHeader* header = reinterpret_cast<PacketHeader*>(sendOv->buffer);
        memcpy(copyOv->buffer, sendOv->buffer, header->size);

        // 전송 시도
        SendPacket(session, copyOv);
    }

    // 원본은 반납
    GMemoryPool->Push(sendOv);
}


void SessionManager::BroadcastNewUser(Session* newUser) {

    std::vector<Session*> activeSessions = GetSessionsSnapshot();

    // 2. 루프 밖에서 패킷 생성 (공용 데이터)
    OverlappedEx* sendOv = GMemoryPool->Pop();
    memset(sendOv, 0, sizeof(OverlappedEx));
    sendOv->type = IO_TYPE::SEND;

    S2C_EnterUserPacket* pkt = reinterpret_cast<S2C_EnterUserPacket*>(sendOv->buffer);
    pkt->header.size = sizeof(S2C_EnterUserPacket);
    pkt->header.id = static_cast<uint16_t>(Packet_S2C::ENTER_USER);
    pkt->userUid = newUser->userUid;
    pkt->x = newUser->x;
    pkt->y = newUser->y;

    // 3. 락이 없는 상태에서 안전하게 브로드캐스트 수행
    for (Session* other : activeSessions) {
        if (other == newUser) continue;
        if (other->isFree) continue; // 그 사이 나갔을 경우를 대비한 방어 코드

        // [A] 기존 유저에게 새 유저 소식 알림
        OverlappedEx* copyOv = GMemoryPool->Pop();
        memcpy(copyOv, sendOv, sizeof(OverlappedEx));
        SendPacket(other, copyOv);

        // [B] 새 유저에게 기존 유저(other)의 정보 전송
        OverlappedEx* existingUserOv = GMemoryPool->Pop();
        memset(existingUserOv, 0, sizeof(OverlappedEx));
        existingUserOv->type = IO_TYPE::SEND;

        S2C_EnterUserPacket* ePkt = reinterpret_cast<S2C_EnterUserPacket*>(existingUserOv->buffer);
        ePkt->header.size = sizeof(S2C_EnterUserPacket);
        ePkt->header.id = static_cast<uint16_t>(Packet_S2C::ENTER_USER);
        ePkt->userUid = other->userUid;
        ePkt->x = other->x;
        ePkt->y = other->y;

        SendPacket(newUser, existingUserOv);
    }

    GMemoryPool->Push(sendOv); // 원본 반납
}



void SessionManager::BroadcastLeaveUser(int32_t leavingUserUid) {
   // if (leavingUser->userUid == -1) return;
    if (leavingUserUid == -1) return;



    //1.저ㅗㄴ송할 패킷 구성
    S2C_LeaveUserPacket leavePkt;
    leavePkt.header.id = static_cast<uint16_t>(Packet_S2C::LEAVE_USER);
    leavePkt.header.size = sizeof(S2C_LeaveUserPacket);
    leavePkt.userUid = leavingUserUid;

    // 2. 락 경합 최소화를 위한 스냅샷 생성
    // m_activeMutex를 아주 잠깐만 잡고 리스트를 복사합니다.
    std::vector<Session*> targets = GetSessionsSnapshot();


    // 2. 현재 접속 중인 모든 유저에게 전송 (스냅샷 활용)
    // 원본 패킷(sendOv)을 Broadcast 함수에 넣어서 처리
   // this->Broadcast(sendOv);
    // 3. 전송 루프
    for (Session* target : targets) {
        // 유효성 체크 및 자기 자신 제외
        if (target->isFree.load() || target->userUid == leavingUserUid)
            continue;

        // 전송용 OverlappedEx 생성
        OverlappedEx* sendOv = GMemoryPool->Pop();
        if (sendOv == nullptr) continue;

        memset(sendOv, 0, sizeof(OVERLAPPED));
        sendOv->type = IO_TYPE::SEND;
        memcpy(sendOv->buffer, &leavePkt, sizeof(S2C_LeaveUserPacket));

      
        // Gather Write가 적용된 SendPacket 호출
        // 이제 120명 상황에서도 시스템 콜 부담 없이 순식간에 알림이 전달됩니다.
        SendPacket(target, sendOv);
    }


}


void SendPacket(Session* session, OverlappedEx* sendOv) {
    bool failed = false;
    bool initiateSend = false;

    // Gather 전송을 위한 준비
    WSABUF wsaBufs[WSA_BUFF_CNT];
    int bufCount = 0;
    OverlappedEx* firstTarget = nullptr;

    {
        std::lock_guard<std::mutex> lock(session->sendMutex);

        if (session->isFree.load() || session->socket == INVALID_SOCKET) {
            GMemoryPool->Push(sendOv);
            return;
        }

        // 1. 일단 큐에 넣기
        session->sendQueue.push_back(sendOv);

        // 2. 현재 전송 중이 아니라면, 큐에 쌓인 것들을 최대한 긁어서 전송 시작
        if (!session->isSending) {
            session->isSending = true;
            initiateSend = true;

            // 큐에 있는 패킷들을 최대 32개까지 Gather
            for (auto* ov : session->sendQueue) {
                if (bufCount >= 32) break;

                wsaBufs[bufCount].buf = ov->buffer;
                wsaBufs[bufCount].len = reinterpret_cast<PacketHeader*>(ov->buffer)->size;

                if (bufCount == 0) firstTarget = ov;
                bufCount++;
            }

            // [중요] 이번 전송에 포함된 패킷 개수를 기록!
            session->sendPendingCount = bufCount;
        }
    }

    // 3. 락 밖에서 전송 호출
    if (initiateSend && firstTarget) {
        DWORD sentBytes = 0;
        // bufCount 만큼의 배열을 한 번에 보냄
        if (WSASend(session->socket, wsaBufs, bufCount, &sentBytes, 0, (LPOVERLAPPED)firstTarget, NULL) == SOCKET_ERROR) {
            int errCode = WSAGetLastError();
            if (errCode != ERROR_IO_PENDING) {
                {
                    std::lock_guard<std::mutex> lock(session->sendMutex);
                    session->isSending = false;
                    session->sendPendingCount = 0; // 실패했으므로 카운트 리셋
                }
                failed = true;
                // HandleDisconnect(session); // 필요 시 호출
            }
        }
    }
}


void HandleDisconnect(Session* session) {
    if (session == nullptr) return;

    // 1. 중복 종료 방지
    bool expected = false;
    if (!session->isFree.compare_exchange_strong(expected, true)) {
        return;
    }

    // 2. 퇴장 알림을 위한 UID 백업 (세션이 초기화되기 전에 수행)
    int32_t backupUid = session->userUid;

    // 3. 소켓 닫기 (커널 신호 차단)
    if (session->socket != INVALID_SOCKET) {
        closesocket(session->socket);
        session->socket = INVALID_SOCKET;
    }

    // 4. 활성 목록 제거 및 스택 반납
    // 내부에서 swap-back(O(1))으로 제거되고 프리 스택으로 들어갑니다.
    GSessionManager.Release(session);

    // 5. [핵심] 다른 유저들에게 퇴장 알림 전송
    // Release 이후에 호출하여 목록 정리가 끝났음을 보장합니다.
    if (backupUid != -1) {
        GSessionManager.BroadcastLeaveUser(backupUid);
    }
}