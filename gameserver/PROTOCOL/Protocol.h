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


    MONSTER_MOVE = 6,
    MONSTER_ENTER ,
    MONSTER_LEAVE,

    ALL_LOCATE ,
    MONSTER_SPAWN ,

    SECTOR_ENTER_PLAYER_LIST ,
    SECTOR_LEAVE_PLAYER_LIST ,

    SECTOR_ENTER_PLAYER ,
    SECTOR_LEAVE_PLAYER ,



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
    //int32_t userUid;
    int32_t monsterId;
    int32_t x, y;
    float speed;

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



//////섹터 기반 이동......

// sector 유저 이동.....
struct PlayerEntryInfo {
    int32_t userUid;
    int32_t x, y;
    float speed;
    // 필요한 추가 정보 (직업, 외형 등)
};
struct S2C_EnterPlayerListPacket {
    PacketHeader header;
    uint16_t playerCount;
    // 이후 PlayerEntryInfo 데이터가 playerCount만큼 붙음
};


struct S2C_LeavePlayerListPacket {
    PacketHeader header;
    uint16_t playerCount;
    // 이후 int32_t userUid 데이터가 playerCount만큼 붙음
};


struct S2C_MovePlayerPacket
{
    PacketHeader header;

    PlayerEntryInfo playerInfo;

};


struct S2C_EnterPlayerPacket
{
    PacketHeader header;

    PlayerEntryInfo playerInfo;

};


struct S2C_LeavePlayerPacket
{
    PacketHeader header;
    int32_t userUid;
};


// sector 몬스터 이동.....
struct MonsterEntryInfo {
    int32_t monsterUid;
    int32_t x, y;
    float speed;
    // 필요한 추가 정보 (직업, 외형 등)
};

struct S2C_MoveMonsterPacket
{
    PacketHeader header;

    MonsterEntryInfo monsterInfo;

};

struct S2C_EnterMonsterPacket
{
    PacketHeader header;

    MonsterEntryInfo monsterInfo;

};

struct S2C_LeaveMonsterPacket
{
    PacketHeader header;

    int32_t monsterUid;
};


#pragma pack(pop)