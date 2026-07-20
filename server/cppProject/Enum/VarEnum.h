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