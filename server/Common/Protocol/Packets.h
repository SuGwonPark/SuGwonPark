#pragma once
#include <cstdint>
#include "Protocol/PacketID.h"

#pragma pack(push, 1)

struct PacketHeader {
	uint16_t size;
	uint16_t id;
};

// ===== 로그인 =====
struct REQ_LoginPacket {
	PacketHeader header;
	char userID[32];
	char password[32];
};

struct RES_LoginPacket {
	PacketHeader header;
	uint64_t userUID;
	bool success;
	char message[64];
};

// ===== 로비 =====
struct REQ_CharacterListPacket {
	PacketHeader header;
};

struct CharacterSummary {        // 목록 한 줄
	uint64_t playerID;
	uint8_t  slot;
	char     name[16];
	uint16_t level;
};

struct RES_CharacterListPacket {
	PacketHeader header;
	uint8_t count;
	// CharacterSummary list[count];  (가변 길이)
};

struct REQ_SelectCharacterPacket {
	PacketHeader header;
	uint64_t playerID;
};

struct RES_SelectCharacterPacket {
	PacketHeader header;
	bool success;
};

// ===== 게임 =====
struct REQ_ReadyToSpawnPacket {
	PacketHeader header;
	uint64_t playerID;   // Gateway가 덮어씀 (클라 값은 신뢰 안 함)
};

struct MovePacket {
	PacketHeader header;
	uint64_t playerID;
	float x;
	float y;
	float z;
};

struct AttackPacket {
	PacketHeader header;
	uint64_t playerID;
	int32_t targetId;
	int32_t damage;
};

#pragma pack(pop)