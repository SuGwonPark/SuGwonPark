#pragma once
#include <cstdint>
#include "PacketID.h"
#include "ProtocolTypes.h"   
#include "VarEnum.h"


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

struct REQ_CreateCharacterPacket
{
    PacketHeader header;
    char name[50]; // UTF-8, 널 종료 보장 안 됨
};

struct RES_CreateCharacterPacket
{
    PacketHeader header;
    CreateCharacterResult result;
    CharacterSummary character; // Success일 때만 유효
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

struct REQ_AttackPacket {
	PacketHeader header;
	ObjectType   targetType;   // 타겟 타이 
	uint64_t     targetUID;    // 타겟 고유값
};

struct RES_AttackPacket {        // 공격 결과. 존 전체에 브로드캐스트
	PacketHeader header;
	ObjectType   attackerType; // 4단계(몬스터 반격)에서 Monster도 사용
	uint64_t     attackerUID;
	ObjectType   targetType;
	uint64_t     targetUID;
	int32_t      damage;       // 실제로 깎인 양
	int32_t      targetHp;     // 맞은 뒤 남은 HP
};

struct RES_MonsterHpPacket {
	PacketHeader header;
	uint64_t monsterUID;
	int32_t  hp;
};

struct RES_ExpPacket {
	PacketHeader header;
	uint64_t gainedExp;    // 이번에 얻은 양
	uint64_t totalExp;     // 누적
};

struct REQ_RevivePacket {
	PacketHeader header;
};

struct RES_RevivePacket {
	PacketHeader header;
	uint64_t playerID;
	float    x, y, z;      // 부활 위치
	int32_t  hp;
};

struct REQ_SkillCastPacket {
	PacketHeader header;
	uint32_t   skillID;
	ObjectType targetType;       // Single 스킬일 때만 사용
	uint64_t   targetUID;
};

struct RES_SkillCastPacket {     // 가변 길이: header + ... + SkillHit[hitCount]
	PacketHeader header;
	uint64_t casterID;
	uint32_t skillID;
	int32_t  casterHp;           // 시전 후 남은 MP
	int32_t casterMp;
	uint16_t hitCount;
};

struct RES_SkillFailPacket {
	PacketHeader header;
	uint32_t        skillID;
	SkillFailReason reason;
};

struct RES_PlayerStatPacket {
	PacketHeader header;
	uint64_t playerID;
	int32_t  hp;
	int32_t  maxHp;
	int32_t  mp;
	int32_t  maxMp;
};

struct RES_LevelUpPacket
{
    PacketHeader header;
    uint64_t playerID;
    uint16_t level;
    int32_t maxHp;
    int32_t hp; // 레벨업 시 가득 참
    int32_t maxMp;
    int32_t mp;
};

#pragma pack(pop)