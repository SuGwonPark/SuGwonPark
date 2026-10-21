#pragma once
#include <cstdint>
#include "PacketID.h"
#include "ProtocolTypes.h"   // ObjectType, PlayerInfo, CharacterSummary, MonsterInfo

// 요청(REQ_)/응답(RES_) 패킷만 둔다. 패킷 안에 들어가는 타입은 ProtocolTypes.h

#pragma pack(push, 1)

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


struct RES_MonsterSpawnPacket {
	PacketHeader header;
	uint16_t count;
};

struct RES_MonsterDespawnPacket {
	PacketHeader header;
	uint64_t monsterUID;
};

struct RES_MonsterMovePacket {
	PacketHeader header;
	uint64_t monsterUID;
	float x, y, z;
};

#pragma pack(pop)