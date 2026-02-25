#include "PacketHandler.h"
#include "..\Session\SessionManager.h"
#include "Protocol.h"

void PacketHandler::Handle_C2S_LOGIN(Session* session, PacketHeader* header)
{
    // 1. 패킷 데이터 해석 (아이디 등)
    // 실제로는 구조체를 만들어서 처리하는 게 좋습니다.
    char* loginData = reinterpret_cast<char*>(header) + sizeof(PacketHeader);

    // 2. 인증 로직 (원래는 DB 조회를 해야 함)
    // 지금은 무조건 성공이라고 가정하고 UID 부여

    static volatile long long  usertestid = 0;

    long long newId = InterlockedIncrement64(&usertestid);
    session->userUid = (int32_t)newId; // 예시 ID
    session->nickname = L"TestUser";
    session->isAuth = true;

   // std::cout << "유저 로그인 성공: " << session->userUid << std::endl;

    // 3. 클라이언트에게 결과 전송 (S2C_LOGIN_RESULT)
    OverlappedEx* sendOv = GMemoryPool->Pop();
    memset(sendOv, 0, sizeof(OverlappedEx));
    sendOv->type = IO_TYPE::SEND;

    S2C_LoginResult* resPkt = reinterpret_cast<S2C_LoginResult*>(sendOv->buffer);
    resPkt->header.size = sizeof(S2C_LoginResult);
    resPkt->header.id = static_cast<uint16_t>(Packet_S2C::LOGIN_RESULT);

    resPkt->success = true; // 이동한 유저의 식별자
    resPkt->userUid = newId; //session->userUid; // 이동한 유저의 식별자
    resPkt->x = session->x;
    resPkt->y = session->y;

   // bool result = true;
   // memcpy(sendOv->buffer + sizeof(PacketHeader), &result, sizeof(bool));

    SendPacket(session, sendOv);
}


void PacketHandler::Handle_C2S_CHAT(Session* session, PacketHeader* header) {
    // 1. 패킷 내용 추출 (헤더 뒤의 데이터)
    char* chatMsg = reinterpret_cast<char*>(header) + sizeof(PacketHeader);

   // C2S_MoveP* pkt = reinterpret_cast<C2S_MovePacket*>(header);



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

void PacketHandler::Handle_C2S_MOVE(Session* session, PacketHeader* header)
{

    // 1. 패킷 데이터 캐스팅
    C2S_MovePacket* pkt = reinterpret_cast<C2S_MovePacket*>(header);

    // 2. 서버 내 유저 정보 갱신 (보통 여기서 이동이 가능한 지형인지 검증 로직이 들어감)
    session->x = pkt->x;
    session->y = pkt->y;
   // session->yaw = pkt->yaw;

    // 3. 주변 유저에게 알림 (지금은 전체 유저에게 브로드캐스트)
    OverlappedEx* sendOv = GMemoryPool->Pop();
    memset(sendOv, 0, sizeof(OverlappedEx));
    sendOv->type = IO_TYPE::SEND;

    // S2C_MovePacket 구조체 크기에 맞춰 버퍼 세팅
    S2C_MovePacket* resPkt = reinterpret_cast<S2C_MovePacket*>(sendOv->buffer);
    resPkt->header.size = sizeof(S2C_MovePacket);
    resPkt->header.id = static_cast<uint16_t>(Packet_S2C::MOVE_RES);

    resPkt->userUid = session->userUid; // 이동한 유저의 식별자
    resPkt->x = session->x;
    resPkt->y = session->y;
   // resPkt->yaw = session->yaw;

    // 모든 사람에게 전송 (나 자신을 포함해도 되고 제외해도 되지만, 보통 확인을 위해 포함)
    GSessionManager.Broadcast(sendOv);


}


void PacketHandler::Handle_C2S_ENTER(Session* session, PacketHeader* header) {

    if (session->isAuth == false) {
        closesocket(session->socket);
        session->socket = INVALID_SOCKET;
        return;
    }

    // 1. 패킷 데이터 캐스팅
   // S2C_EnterUserPacket* pkt = reinterpret_cast<S2C_EnterUserPacket*>(header);

    // 1. 유저 정보 검증 및 데이터 로드 (DB 등)
    //session->userUid = pkt->userUid;
  //  session->nickname = pkt->nickname;
    //session->x = 0.0f; // 시작 좌표 설정
   // session->y = 0.0f;
    session->isAuth = true; // 인증 완료 플래그

    // 2. 이 세션을 '활성 목록'에 추가 (Broadcasting 대상이 됨)
   // GSessionManager.AddActiveSession(session);

    // 3. ★ 여기서 호출!
    // 주변 유저들에게 "나 들어왔어!"라고 알림
    GSessionManager.BroadcastNewUser(session);

    // 4. 본인에게는 성공 응답 패킷 전송
    // Send_S2C_EnterGameOk(session);
}