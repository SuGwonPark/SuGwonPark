#pragma once

struct PacketHeader {
	uint16_t size;
	uint16_t type;
};

// 로그인 요청
struct LoginReqPacket {
	PacketHeader header;
	char userId[32];
	char password[32];
};

// 로그인 응답
struct LoginResPacket {
	PacketHeader header;
	bool success;
	int32_t playerId;
	char message[64];   // "로그인 성공" or "비밀번호 오류" 등
};

// 방 입장 요청
struct RoomJoinReqPacket {
	PacketHeader header;
	int32_t playerId;
	int32_t roomId;
};

// 방 입장 응답
struct RoomJoinResPacket {
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
