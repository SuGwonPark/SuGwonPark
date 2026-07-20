// PacketHandler.cpp
#include "pch.h"
#include "PacketHandler.h"
#include "Session.h"
#include "Manager/PlayerManager.h"
#include "Manager/RoomManager.h"
#include "ENUM/VarEnum.h"



void PacketHandler::Handle(std::shared_ptr<Session> session,
	const char* data, std::size_t length) {
	if (length < sizeof(PacketHeader)) return;

	PacketHeader header;
	std::memcpy(&header, data, sizeof(PacketHeader));

	switch (header.type) {
	case PKT_LOGIN_REQ: {
		if (length < sizeof(LoginReqPacket)) return;
		HandleLogin(session, reinterpret_cast<const LoginReqPacket*>(data));
		break;
	}
	case PKT_ROOM_JOIN_REQ: {
		if (length < sizeof(RoomJoinReqPacket)) return;
		HandleRoomJoin(session, reinterpret_cast<const RoomJoinReqPacket*>(data));
		break;
	}
	case PKT_MOVE: {
		if (length < sizeof(MovePacket)) return;
		HandleMove(session, reinterpret_cast<const MovePacket*>(data));
		break;
	}
	default:
		std::cout << "알 수 없는 패킷: " << header.type << std::endl;
		break;
	}
}

void PacketHandler::HandleLogin(std::shared_ptr<Session> session,
	const LoginReqPacket* pkt) {
	// 실제로는 DB 조회해야 하지만 지금은 간단히 검증
	bool success = (std::string(pkt->password) == "1234");

	LoginResPacket res;
	res.header.size = sizeof(LoginResPacket);
	res.header.type = PKT_LOGIN_RES;
	res.success = success;

	if (success) {
		static int nextId = 1;
		res.playerId = nextId++;
		strcpy_s(res.message, sizeof(res.message), "로그인 성공");
		res.message[sizeof(res.message) - 1] = '\0';

		// 세션 상태 업데이트
		session->playerId_ = res.playerId;
		session->state_ = SessionState::INLOBBY;

		// PlayerManager에 등록
		PlayerManager::GetInstance().AddPlayer(res.playerId, pkt->userId);
	}
	else {
		res.playerId = -1;
		strcpy_s(res.message, sizeof(res.message), "비밀번호 오류");
	}

	session->Send(reinterpret_cast<const char*>(&res), sizeof(res));
}

void PacketHandler::HandleRoomJoin(std::shared_ptr<Session> session,
	const RoomJoinReqPacket* pkt) {
	auto room = RoomManager::GetInstance().GetRoom(pkt->roomId);

	RoomJoinResPacket res;
	res.header.size = sizeof(RoomJoinResPacket);
	res.header.type = PKT_ROOM_JOIN_RES;

	if (!room) {
		res.success = false;
		res.roomId = -1;
		strcpy_s(res.message, sizeof(res.message), "방이 존재하지 않습니다");
	}
	else if (room->IsFull()) {
		res.success = false;
		res.roomId = -1;
		strcpy_s(res.message, sizeof(res.message), "방이 가득 찼습니다");
	}
	else {
		room->Join(session);
		session->roomId_ = pkt->roomId;
		session->state_ = SessionState::INROOM;

		res.success = true;
		res.roomId = pkt->roomId;
		strcpy_s(res.message, sizeof(res.message), "입장 성공");
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
	auto room = RoomManager::GetInstance().GetRoom(session->roomId_);
	if (room) {
		room->Broadcast(
			reinterpret_cast<const char*>(pkt),
			sizeof(MovePacket),
			pkt->playerId   // 본인 제외
		);
	}
}