#pragma once

enum PacketType : uint16_t {
	// 로그인
	PKT_LOGIN_REQ = 1,   // 클라이언트 → 서버 (로그인 요청)
	PKT_LOGIN_RES = 2,   // 서버 → 클라이언트 (로그인 결과)

	// 로비
	PKT_ROOM_LIST_REQ = 3,  // 방 목록 요청
	PKT_ROOM_LIST_RES = 4,  // 방 목록 응답

	// 게임방
	PKT_ROOM_JOIN_REQ = 5,  // 방 입장 요청
	PKT_ROOM_JOIN_RES = 6,  // 방 입장 결과
	PKT_ROOM_LEAVE = 7,  // 방 퇴장

	// 게임 중
	PKT_MOVE = 8,
	PKT_ATTACK = 9,
};
