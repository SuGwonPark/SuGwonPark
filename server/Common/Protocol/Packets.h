#pragma once
#include <cstdint>
#include "PacketID.h"

#pragma pack(push, 1)
struct PlayerInfo {              // 다른 유저에게 보여줄 최소 정보
	uint64_t playerID;
	char     name[50];
	float    x, y, z;
	int32_t  hp;
};


//////////// 패킷 ////////////////
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
	char message[50];
};

// ===== 로비 =====
struct REQ_CharacterListPacket {
	PacketHeader header;
};

struct CharacterSummary {        // 목록 한 줄
	uint64_t playerID;
	uint64_t exp;
	char     name[50];
};

struct RES_CharacterListPacket {
	PacketHeader header;
	uint8_t count;
};

struct REQ_SelectCharacterPacket {
	PacketHeader header;
	uint64_t playerID;
};

struct RES_SelectCharacterPacket {
	PacketHeader header;
	uint32_t zoneID;
	bool success;
};

struct RES_EnterGamePacket {     // 나 자신의 입장 결과
	PacketHeader header;
	uint32_t   zoneID;
	uint32_t   channelID;
	PlayerInfo me;
	int32_t    mp;
	uint64_t   exp;
};

struct RES_SpawnPacket {         // 가변 길이: header + count + PlayerInfo[count]
	PacketHeader header;
	uint16_t count;
};

struct RES_DespawnPacket {
	PacketHeader header;
	uint64_t playerID;
};

struct REQ_ReadyToSpawnPacket {
	PacketHeader header;
	uint64_t playerID;   // Gateway가 덮어씀 (클라 값은 신뢰 안 함)
};

struct REQ_PortalPacket {
	PacketHeader header;
	uint32_t portalID;
};

struct RES_ZoneChangePacket {
	PacketHeader header;
	uint32_t zoneID;
	uint32_t channelID;
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