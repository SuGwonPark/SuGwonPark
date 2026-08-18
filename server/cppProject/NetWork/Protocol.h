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

// 방 입장 요청
struct REQ_RoomJoinPacket {
	PacketHeader header;
	int32_t playerId;
	int32_t roomId;
};

struct RES_RoomJoinPacket {
	PacketHeader header;
	bool success;
	int32_t roomId;
	char message[64];
};

// 이동 패킷
struct MovePacket {
	PacketHeader header;
	int32_t playerId;
	float x;
	float y;
};

// 공격 패킷
struct AttackPacket {
	PacketHeader header;
	int32_t playerId;
	int32_t targetId;
	int32_t damage;
};




// 패킷 ID 정의 예시
enum PacketID : uint16_t {
	// 이동 및 전투 (핸드오버 중 폐기 대상)
	PKT_C_MOVE = 1001,
	PKT_C_ATTACK = 1002,
	PKT_C_SKILL_CAST = 1003,

	PKT_S_ZONE_LEAVE_COMPLETED = 2001, // 존 이동 완료 처리


	// 채팅 및 소셜 (핸드오버 중 즉시 ChatServer로 우회)
	PKT_C_CHAT = 3001,
	PKT_C_WHISPER = 3002,

	// 시스템 및 핸드오버 라이프사이클 (핸드오버 중 보류 후 Zone 2로 방출)
	PKT_C_READY_TO_SPAWN = 4001,
	PKT_C_USE_ITEM = 4002
};
