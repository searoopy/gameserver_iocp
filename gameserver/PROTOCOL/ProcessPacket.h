#pragma once


struct Session;
struct PacketHeader;

void ProcessPacket(Session* session, PacketHeader* header);