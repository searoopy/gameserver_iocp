#include "ProcessPacket.h"

#include "protocol.h"
#include "..\Common.h"



void ProcessPacket(Session* session, PacketHeader* header) {


    Packet_C2S packetId = static_cast<Packet_C2S>(header->id);


    // 로그인이 안 된 유저가 로그인 이외의 패킷을 보낸 경우
    if (session->isAuth == false && packetId != Packet_C2S::LOGIN) {
        std::cout << "미인증 유저의 비정상 접근 차단!" << std::endl;
        return;
    }


    switch (packetId)
    {

        case Packet_C2S::LOGIN:
        {

            PacketHandler::Handle_C2S_LOGIN(session, header);
        }
        break;

        case Packet_C2S::CHAT :
        {
            // 여기에 실제 게임 로직 (채팅 전달, 이동 처리 등) 작성
            // 현재는 테스트를 위해 수신 알림만 출력
            std::cout << "패킷 처리 중... ID: " << header->id << ", Size: " << header->size << std::endl;


            //GSessionManager.Broadcast( session->)
            PacketHandler::Handle_C2S_CHAT(session, header);
       

           
        }
        break;

        case Packet_C2S::HEARTBEAT:
        {


            session->lastHeartbeatTick = GetTickCount64();

            //std::cout << "hearbeat recived from session\n";
        }
        break;
    }
}