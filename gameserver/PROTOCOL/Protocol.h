#pragma once
#include <WinSock2.h>
#include <iostream>
#include <cstdint>

#pragma pack(push, 1) // 구조체 멤버 정렬을 1바이트 단위로 (패딩 제거)
struct PacketHeader {
	uint16_t size; // 패킷 전체 크기 (헤더 포함)
	uint16_t id;   // 패킷 종류 (ID)
};
#pragma pack(pop)




// Client -> Server
enum class Packet_C2S : uint16_t {
    LOGIN = 1,
    CHAT = 2,
    MOVE = 3,


    HEARTBEAT = 999,
};



// Server -> Client
enum class Packet_S2C : uint16_t {
    LOGIN_RESULT = 1,
    CHAT_RES = 2, // 브로드캐스트용 채팅
    MOVE_RES = 3, // 유저 이동 정보 전송
};

