#pragma once

#include "..\Common.h"
#include "..\MemoryPool.h"
#include "Protocol.h"
#include "..\Session\Session.h"
#include "..\Monster\Monster.h"

//struct OverlappedEx;

//class Session;
//class Monster;

namespace PACKET
{


    inline OverlappedEx* CreateMovePacket(Session* target) {
        OverlappedEx* sendOv = GMemoryPool->Pop();
        sendOv->Init();
        sendOv->type = IO_TYPE::SEND;
        sendOv->refCount.store(0); // 나 혼자만 보낼 것이므로 1로 설정

        S2C_MovePlayerPacket* pkt = reinterpret_cast<S2C_MovePlayerPacket*>(sendOv->buffer);
        pkt->header.id = static_cast<uint16_t>(Packet_S2C::MOVE_RES);
        pkt->playerInfo.userUid = target->userUid;
        pkt->playerInfo.x = target->x;
        pkt->playerInfo.y = target->y;
        pkt->playerInfo.speed = target->speed;
        // ... 기타 외형 정보 등
        pkt->header.size = sizeof(S2C_MovePlayerPacket);

        return sendOv;
    }


    inline OverlappedEx* CreateEnterPacket(Session* target) {
        OverlappedEx* sendOv = GMemoryPool->Pop();
        sendOv->Init();
        sendOv->type = IO_TYPE::SEND;
        sendOv->refCount.store(0); // 나 혼자만 보낼 것이므로 1로 설정

        S2C_EnterPlayerPacket* pkt = reinterpret_cast<S2C_EnterPlayerPacket*>(sendOv->buffer);
        pkt->header.id = static_cast<uint16_t>(Packet_S2C::SECTOR_ENTER_PLAYER);
        pkt->playerInfo.userUid = target->userUid;
        pkt->playerInfo.x = target->x;
        pkt->playerInfo.y = target->y;
        pkt->playerInfo.speed = target->speed;
        // ... 기타 외형 정보 등
        pkt->header.size = sizeof(S2C_EnterPlayerPacket);

        return sendOv;
    }


    inline OverlappedEx* CreateLeavePacket(int32_t leavingUserUid) {
        OverlappedEx* sendOv = GMemoryPool->Pop();
        sendOv->Init();
        sendOv->type = IO_TYPE::SEND;
        sendOv->refCount.store(0); // 나 혼자만 보낼 것이므로 1로 설정

        S2C_LeavePlayerPacket* pkt = reinterpret_cast<S2C_LeavePlayerPacket*>(sendOv->buffer);
        pkt->header.id = static_cast<uint16_t>(Packet_S2C::SECTOR_LEAVE_PLAYER);
        pkt->userUid = leavingUserUid;

        // ... 기타 외형 정보 등
        pkt->header.size = sizeof(S2C_LeavePlayerPacket);
        return sendOv;
    }

    inline OverlappedEx* CreateLeavePacket(Session* target) {
        /*
        OverlappedEx* sendOv = GMemoryPool->Pop();
        sendOv->Init();
        sendOv->type = IO_TYPE::SEND;
        sendOv->refCount.store(1); // 나 혼자만 보낼 것이므로 1로 설정

        S2C_LeavePlayerPacket* pkt = reinterpret_cast<S2C_LeavePlayerPacket*>(sendOv->buffer);
        pkt->header.id = static_cast<uint16_t>(Packet_S2C::SECTOR_LEAVE_PLAYER);
        pkt->userUid = target->userUid;
    

        // ... 기타 외형 정보 등
        pkt->header.size = sizeof(S2C_LeavePlayerPacket);

        */


        OverlappedEx* sendOv = CreateLeavePacket( static_cast<int32_t>(target->userUid));

        return sendOv;
    }


    inline OverlappedEx* CreateMonsterEnterPacket(Monster* target) {
        OverlappedEx* sendOv = GMemoryPool->Pop();
        sendOv->Init();
        sendOv->type = IO_TYPE::SEND;
        sendOv->refCount.store(0); // 나 혼자만 보낼 것이므로 1로 설정

        S2C_EnterMonsterPacket* pkt = reinterpret_cast<S2C_EnterMonsterPacket*>(sendOv->buffer);
        pkt->header.id = static_cast<uint16_t>(Packet_S2C::MONSTER_ENTER);
        pkt->monsterInfo.monsterUid = target->monsterId;
        pkt->monsterInfo.x = target->pos.x;
        pkt->monsterInfo.y = target->pos.y;
        pkt->monsterInfo.speed = target->speed;
        // ... 기타 외형 정보 등
        pkt->header.size = sizeof(S2C_EnterMonsterPacket);

        return sendOv;
    }




    inline  OverlappedEx* CreateMonsterMovePacket(Monster* target) {
        OverlappedEx* sendOv = GMemoryPool->Pop();
        sendOv->Init();
        sendOv->type = IO_TYPE::SEND;
        sendOv->refCount.store(0); // 나 혼자만 보낼 것이므로 1로 설정

        S2C_MoveMonsterPacket* pkt = reinterpret_cast<S2C_MoveMonsterPacket*>(sendOv->buffer);
        pkt->header.id = static_cast<uint16_t>(Packet_S2C::MONSTER_MOVE);
        pkt->monsterInfo.monsterUid = target->monsterId;
        pkt->monsterInfo.x = target->pos.x;
        pkt->monsterInfo.y = target->pos.y;
        pkt->monsterInfo.speed = target->speed;
        // ... 기타 외형 정보 등
        pkt->header.size = sizeof(S2C_MoveMonsterPacket);

        return sendOv;
    }




    inline  OverlappedEx* CreateMonsterLeavePacket(Monster* target) {
        OverlappedEx* sendOv = GMemoryPool->Pop();
        sendOv->Init();
        sendOv->type = IO_TYPE::SEND;
        sendOv->refCount.store(0); // 

        S2C_LeaveMonsterPacket* pkt = reinterpret_cast<S2C_LeaveMonsterPacket*>(sendOv->buffer);
        pkt->header.id = static_cast<uint16_t>(Packet_S2C::MONSTER_LEAVE);
        pkt->monsterUid = target->monsterId;
        // ... 기타 외형 정보 등
        pkt->header.size = sizeof(S2C_LeaveMonsterPacket);

        return sendOv;
    }

    


    



}