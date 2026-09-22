#pragma once

struct PacketHeader {
	uint16_t size;
	uint16_t id;
};

// 로그인
struct REQ_LoginPacket {
	PacketHeader header;
	char userId[32];
	char password[32];
};

struct RES_LoginPacket {
	PacketHeader header;
	bool success;
	int32_t playerId;
	char message[64];   // "로그인 성공" or "비밀번호 오류" 등
};


// 이동 패킷
struct MovePacket {
	PacketHeader header;
	int32_t playerId;
	float x;
	float y;
	float z;
};

// 공격 패킷
struct AttackPacket {
	PacketHeader header;
	int32_t playerId;
	int32_t targetId;
	int32_t damage;
};




// 패킷 ID 정의 모음
enum PacketID : uint16_t {
	// 이동 및 전투
	PKT_C_MOVE = 1001,
	PKT_C_ATTACK = 1002,
	PKT_C_SKILL_CAST = 1003,

	PKT_S_ZONE_LEAVE_COMPLETED = 2001, // 존 이동 완료 처리


	// 채팅 및 소셜
	PKT_C_CHAT = 3001,
	PKT_C_WHISPER = 3002,

	// 시스템
	PKT_C_READY_TO_SPAWN = 4001,
	PKT_C_USE_ITEM = 4002,

	// 로그인 및 로비 (인증)
	PKT_C_LOGIN = 5001,
	PKT_S_LOGIN = 5002,
	PKT_C_ROOM_JOIN = 5003,
	PKT_S_ROOM_JOIN = 5004
};


// Zone1 이탈 시 저장 완료 통보 패킷 (Handover ACK, Zone -> Gateway)
// 주의: enum 값 PKT_S_ZONE_LEAVE_COMPLETED와 이름이 겹치면 안 되므로 구조체는 별도 이름 사용
struct RES_ZoneLeaveCompletedPacket {
	PacketHeader header;
	uint64_t playerId;
	uint32_t nextZoneId;
};
