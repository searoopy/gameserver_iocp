#include "SessionManager.h"

SessionManager GSessionManager;

void SessionManager::Broadcast(char* buffer, int len) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (Session* session : m_sessions) {
        // 실제로는 여기서 WSASend를 호출하여 모든 세션에 데이터를 보냅니다.
        // 지금은 개념만 잡기 위해 세션 순회만 구현합니다.
    }
}



void SessionManager::Broadcast(OverlappedEx* sendOv) {
    std::vector<Session*> sessionCopy;

    // 1. 락 범위 최소화: 리스트만 빠르게 복사
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        sessionCopy.assign(m_sessions.begin(), m_sessions.end());
    }


    // 2. 락이 풀린 상태에서 루프 실행
    for (Session* session : sessionCopy) {
        // [주의] 여기서 session이 유효한지 보장하려면 shared_ptr가 필요합니다.

        OverlappedEx* copyOv = GMemoryPool->Pop();
        memcpy(copyOv, sendOv, sizeof(OverlappedEx));

        SendPacket(session, copyOv);
    }

    // 원본은 역할을 다했으므로 반납
    GMemoryPool->Push(sendOv);
}



void SendPacket(Session* session, OverlappedEx* sendOv) {
	std::lock_guard<std::mutex> lock(session->sendMutex);

	// 1. 일단 큐에 넣음
	session->sendQueue.push(sendOv);

	// 2. 만약 이미 전송 중이라면 여기서 끝 (완료 스레드가 나중에 처리해줌)
	if (session->isSending) {
		return;
	}

	// 3. 전송 중이 아니라면, 첫 번째 작업을 꺼내서 실제 전송 시작
	session->isSending = true;
	OverlappedEx* target = session->sendQueue.front();

	WSABUF wsaBuf;
	wsaBuf.buf = target->buffer;
	wsaBuf.len = reinterpret_cast<PacketHeader*>(target->buffer)->size;

	DWORD sentBytes = 0;
	if (WSASend(session->socket, &wsaBuf, 1, &sentBytes, 0, (LPOVERLAPPED)target, NULL) == SOCKET_ERROR) {
		if (WSAGetLastError() != ERROR_IO_PENDING) {
			// 에러 처리...
			session->isSending = false;
		}
	}
}


void HandleDisconnect(Session* session) {
    if (session == nullptr) return;

    // 1. 소켓 닫기 (더 이상의 송수신 차단)
    if (session->socket != INVALID_SOCKET) {
        closesocket(session->socket);
        session->socket = INVALID_SOCKET;
    }

    // 2. 세션 매니저에서 제거 (다른 유저들이 이 세션을 참조하지 못하게 함)
    GSessionManager.Remove(session);

    // 3. 메모리 해제
    // 주의: 현재 WorkerThread에서 사용 중인 ovEx(overlapped)는 
    // 이미 GetQueuedCompletionStatus로 뽑아냈으므로 반납하면 되지만,
    // 세션 자체를 삭제할 때는 참조 카운트나 안전한 타이밍을 고려해야 합니다.
    delete session;
}