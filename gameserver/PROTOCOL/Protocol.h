#pragma once
#include <WinSock2.h>
#include <iostream>
#include <cstdint>




// Client -> Server
enum class Packet_C2S : uint16_t {
    LOGIN = 1,
    CHAT = 2,
    MOVE = 3,
    ENTER = 4,
    TARGET_MOVE=5,

    HEARTBEAT = 999,
};



// Server -> Client
enum class Packet_S2C : uint16_t {
    LOGIN_RESULT = 1,
    CHAT_RES = 2, // 브로드캐스트용 채팅
    MOVE_RES = 3, // 유저 이동 정보 전송
    ENTER_USER = 4, //새로운 유저가 입장함.
    LEAVE_USER = 5, ///유저 아웃..
    
    
    MONSTER_MOVE= 6,


    ALL_LOCATE = 7,
    MONSTER_SPAWN =8,
};



#pragma pack(push, 1) // 구조체 멤버 정렬을 1바이트 단위로 (패딩 제거)
struct PacketHeader {
    uint16_t size; // 패킷 전체 크기 (헤더 포함)
    uint16_t id;   // 패킷 종류 (ID)
};

///////////////////////////////////////////////
// client -> server
struct C2S_MovePacket 
{
    PacketHeader header;
    int32_t x, y;
   // float yaw; //바라보는 방향.

};

struct C2S_TargetMovePacket
{
    PacketHeader header;
    int32_t x, y;
   

};

/////////////////////////////////////////////////
//server -> client

struct S2C_LoginResult
{
    PacketHeader header;
    int32_t success;
    int32_t userUid;
    int32_t x, y;
};


struct S2C_MovePacket
{
    PacketHeader header;
    int32_t userUid;
    int32_t x, y;
   // float yaw; //바라보는 방향.

};




struct S2C_EnterUserPacket {
    PacketHeader header;
    int32_t userUid;
    int32_t x, y;
    // 닉네임 등을 추가할 수도 있습니다.
};

struct S2C_LeaveUserPacket 
{
    PacketHeader header;
    int32_t userUid;

};

struct S2C_MonsterMovePacket
{
    PacketHeader header;
    int32_t userUid;
    int32_t monsterId;
    int32_t x, y;

};

struct PlayerPosInfo {
    int32_t uid;
    int32_t tileX;
    int32_t tileY;
};

struct S2C_AllLocationPacket {
    PacketHeader header; // ID: S2C_ALL_LOCATION
    uint16_t playerCount;
    // 이후 PlayerPosInfo 데이터가 연속해서 붙음
};

#pragma pack(pop)