#pragma once
#include <cstdint>

// 클라이언트 <-> 서버 패킷 번호
// 대역: 1000 이동/전투 | 2000 존 | 3000 채팅 | 4000 시스템 | 5000 로그인 | 6000 로비 | 7000 아이템
enum PacketID : uint16_t {
	// 이동 및 전투 (1000~)
	PKT_C_MOVE = 1001,
	PKT_C_ATTACK = 1002,
	PKT_C_SKILL_CAST = 1003,
	PKT_S_MOVE = 1004,

	// 존 (2000~)
	PKT_S_ZONE_LEAVE_COMPLETED = 2001,
	PKT_S_ENTER_GAME = 2002,        // 내 입장 결과
	PKT_S_SPAWN = 2003,             // 다른 플레이어 등장 (목록)
	PKT_S_DESPAWN = 2004,           // 다른 플레이어 퇴장
	PKT_C_PORTAL = 2005,
	PKT_S_ZONE_CHANGE = 2006,

	// 채팅 (3000~)
	PKT_C_CHAT = 3001,
	PKT_C_WHISPER = 3002,

	// 시스템 (4000~)
	PKT_C_READY_TO_SPAWN = 4001,
	PKT_C_USE_ITEM = 4002,

	// 로그인 (5000~)
	PKT_C_LOGIN = 5001,
	PKT_S_LOGIN = 5002,

	// 로비 (6000~)
	PKT_C_CHARACTER_LIST = 6001,
	PKT_S_CHARACTER_LIST = 6002,
	PKT_C_CREATE_CHARACTER = 6003,
	PKT_S_CREATE_CHARACTER = 6004,
	PKT_C_SELECT_CHARACTER = 6005,
	PKT_S_SELECT_CHARACTER = 6006,

	// 아이템 (7000~)
	PKT_C_ENHANCE_ITEM = 7001,
	PKT_S_ENHANCE_ITEM = 7002,
};

