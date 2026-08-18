// PacketHandler.h
#pragma once
#include "Protocol.h"

class Session;

class PacketHandler {
public:
	static void Handle(std::shared_ptr<Session> session,
		const char* data, std::size_t length);

private:
	static void HandleLogin(std::shared_ptr<Session> session,
		const REQ_LoginPacket* pkt);
	static void HandleRoomJoin(std::shared_ptr<Session> session,
		const REQ_RoomJoinPacket* pkt);
	static void HandleMove(std::shared_ptr<Session> session,
		const MovePacket* pkt);
};