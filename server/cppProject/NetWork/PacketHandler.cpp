// PacketHandler.cpp
#include "pch.h"
#include "PacketHandler.h"
#include "Session.h"
#include "Manager/PlayerManager.h"
#include "Manager/RoomManager.h"
#include "Network/Protocol.h"



void PacketHandler::Handle(std::shared_ptr<Session> session,
	const char* data, std::size_t length) {
	if (length < sizeof(PacketHeader)) return;

	PacketHeader header;
	std::memcpy(&header, data, sizeof(PacketHeader));

	switch (header.id) {
	case PKT_C_MOVE: {
		if (length < sizeof(REQ_LoginPacket)) return;
		HandleLogin(session, reinterpret_cast<const REQ_LoginPacket*>(data));
		break;
	}
	case PKT_C_CHAT: {
		if (length < sizeof(REQ_RoomJoinPacket)) return;
		HandleRoomJoin(session, reinterpret_cast<const REQ_RoomJoinPacket*>(data));
		break;
	}
	case PKT_C_READY_TO_SPAWN: {
		if (length < sizeof(MovePacket)) return;
		HandleMove(session, reinterpret_cast<const MovePacket*>(data));
		break;
	}
	default:
		std::cout << "알 수 없는 패킷: " << header.id << std::endl;
		break;
	}
}

void PacketHandler::HandleLogin(std::shared_ptr<Session> session,
	const REQ_LoginPacket* pkt) {
	// 실제로는 DB 조회해야 하지만 지금은 간단히 검증
	bool success = (std::string(pkt->password) == "1234");

	RES_LoginPacket res;
	res.header.size = sizeof(RES_LoginPacket);
	res.header.id = PKT_C_MOVE;
	res.success = success;

	if (success) {
		static int nextId = 1;
		res.playerId = nextId++;
		strcpy_s(res.message, sizeof(res.message), "로그인 성공");
		res.message[sizeof(res.message) - 1] = '\0';

		// 세션 상태 업데이트
		session->setPlayerID(res.playerId);


		// PlayerManager에 등록
		PlayerManager::GetInstance().AddPlayer(res.playerId, pkt->userId);
	}
	else {
		res.playerId = -1;
		strcpy_s(res.message, sizeof(res.message), "비밀번호 오류");
	}

	session->Send(reinterpret_cast<const char*>(&res), sizeof(res));
}


void PacketHandler::HandleMove(std::shared_ptr<Session> session,
	const MovePacket* pkt) {
	// 플레이어 위치 업데이트
	auto player = PlayerManager::GetInstance().GetPlayer(pkt->playerId);
	if (player) {
		player->Move(pkt->x, pkt->y);
	}

	// 같은 방 플레이어들에게 이동 브로드캐스트
	auto room = RoomManager::GetInstance().GetRoom(session->getRoomID());
	if (room) {
		room->Broadcast(
			reinterpret_cast<const char*>(pkt),
			sizeof(MovePacket),
			pkt->playerId   // 본인 제외
		);
	}
}