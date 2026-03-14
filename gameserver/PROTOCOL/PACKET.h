#pragma once

#include "..\Common.h"
#include "..\MemoryPool.h"
#include "Protocol.h"
#include "..\Session\Session.h"

//struct OverlappedEx;

namespace PACKET
{


    OverlappedEx* CreateMovePacket(Session* target) {
        OverlappedEx* sendOv = GMemoryPool->Pop();
        sendOv->Init();
        sendOv->type = IO_TYPE::SEND;
        sendOv->refCount.store(1); // 나 혼자만 보낼 것이므로 1로 설정

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


    OverlappedEx* CreateEnterPacket(Session* target) {
        OverlappedEx* sendOv = GMemoryPool->Pop();
        sendOv->Init();
        sendOv->type = IO_TYPE::SEND;
        sendOv->refCount.store(1); // 나 혼자만 보낼 것이므로 1로 설정

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



    OverlappedEx* CreateLeavePacket(Session* target) {
        OverlappedEx* sendOv = GMemoryPool->Pop();
        sendOv->Init();
        sendOv->type = IO_TYPE::SEND;
        sendOv->refCount.store(1); // 나 혼자만 보낼 것이므로 1로 설정

        S2C_LeavePlayerPacket* pkt = reinterpret_cast<S2C_LeavePlayerPacket*>(sendOv->buffer);
        pkt->header.id = static_cast<uint16_t>(Packet_S2C::SECTOR_ENTER_PLAYER);
        pkt->userUid = target->userUid;
    

        // ... 기타 외형 정보 등
        pkt->header.size = sizeof(S2C_LeavePlayerPacket);

        return sendOv;
    }

}