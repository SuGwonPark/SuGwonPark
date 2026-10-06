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
	int64_t playerID;
	char message[64];   // "로그인 성공" or "비밀번호 오류" 등
};

// Zone1 이탈 시 저장 완료 통보 패킷 (Handover ACK, Zone -> Gateway)
// 주의: enum 값 PKT_S_ZONE_LEAVE_COMPLETED와 이름이 겹치면 안 되므로 구조체는 별도 이름 사용
struct RES_ZoneLeaveCompletedPacket {
	PacketHeader header;
	uint64_t playerID;
	uint32_t nextZoneID;
};


// 이동 패킷
struct MovePacket {
	PacketHeader header;
	uint64_t playerID;
	float x;
	float y;
	float z;
};

// 공격 패킷
struct AttackPacket {
	PacketHeader header;
	uint64_t playerID;
	int32_t targetId;
	int32_t damage;
};

#pragma pack(push, 1)
// 핸드셰이크에 담을 인스턴스 하나
struct ZoneKeyEntry {
	uint32_t zoneID;
	uint32_t channelID;
};

// 가변 길이 패킷: 헤더 뒤에 ZoneKeyEntry가 count개 이어짐
struct ZoneHandshakePacket {
	PacketHeader header;
	uint16_t count;
	// ZoneKeyEntry entries[count];
};

// 게이트웨이 <-> Zone 서버 간 래퍼 헤더
struct InternalPacketHeader {
	uint16_t size;
	uint16_t id;
	uint64_t sessionID;
	uint32_t zoneID;      // 추가: 목적지(또는 출발지) 인스턴스
	uint32_t channelID;   // 추가
};
#pragma pack(pop)

inline uint64_t MakeZoneKey(uint32_t zoneID, uint32_t channelID) {
	return (static_cast<uint64_t>(zoneID) << 32) | channelID;
}

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
	PKT_S_ROOM_JOIN = 5004,

	// PacketID enum에 추가
	PKT_ZONE_HANDSHAKE = 9997,
};



