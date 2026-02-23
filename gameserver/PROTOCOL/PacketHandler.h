


#pragma once
#include "..\Common.h"


struct Session;

class PacketHandler {
public:

    static void Handle_C2S_LOGIN(Session* session, PacketHeader* header);
    static void Handle_C2S_CHAT(Session* session, PacketHeader* header);
    static void Handle_C2S_MOVE(Session* session, PacketHeader* header);
    static void Handle_C2S_ENTER(Session* session, PacketHeader* header);
    // ... 수많은 패킷 처리 함수들
};