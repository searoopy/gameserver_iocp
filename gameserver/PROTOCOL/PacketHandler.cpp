#include "PacketHandler.h"
#include "..\SessionManager.h"


void PacketHandler::Handle_C2S_LOGIN(Session* session, PacketHeader* header)
{
    // 1. 패킷 데이터 해석 (아이디 등)
    // 실제로는 구조체를 만들어서 처리하는 게 좋습니다.
    char* loginData = reinterpret_cast<char*>(header) + sizeof(PacketHeader);

    // 2. 인증 로직 (원래는 DB 조회를 해야 함)
    // 지금은 무조건 성공이라고 가정하고 UID 부여
    session->userUid = 12345; // 예시 ID
    session->nickname = L"TestUser";
    session->isAuth = true;

    std::cout << "유저 로그인 성공: " << session->userUid << std::endl;

    // 3. 클라이언트에게 결과 전송 (S2C_LOGIN_RESULT)
    OverlappedEx* sendOv = GMemoryPool->Pop();
    memset(sendOv, 0, sizeof(OverlappedEx));
    sendOv->type = IO_TYPE::SEND;

    PacketHeader* sHeader = reinterpret_cast<PacketHeader*>(sendOv->buffer);
    sHeader->size = sizeof(PacketHeader) + sizeof(bool); // 헤더 + 결과값(bool)
    sHeader->id = static_cast<uint16_t>(Packet_S2C::LOGIN_RESULT);

    bool result = true;
    memcpy(sendOv->buffer + sizeof(PacketHeader), &result, sizeof(bool));


    SendPacket(session, sendOv);
}







void PacketHandler::Handle_C2S_CHAT(Session* session, PacketHeader* header) {
    // 1. 패킷 내용 추출 (헤더 뒤의 데이터)
    char* chatMsg = reinterpret_cast<char*>(header) + sizeof(PacketHeader);

    // 2. 보낼 패킷 생성 (S_CHAT 패킷)
    OverlappedEx* sendOv = GMemoryPool->Pop();
    sendOv->type = IO_TYPE::SEND;

    PacketHeader* sHeader = reinterpret_cast<PacketHeader*>(sendOv->buffer);
    sHeader->size = header->size; // 받은 크기 그대로 (헤더 포함)
    sHeader->id = static_cast<uint16_t>(Packet_S2C::CHAT_RES);

    // 메시지 복사
    int payloadSize = header->size - sizeof(PacketHeader);
    memcpy(sendOv->buffer + sizeof(PacketHeader), chatMsg, payloadSize);

    // 3. 모든 사람에게 전송
    GSessionManager.Broadcast(sendOv);
}