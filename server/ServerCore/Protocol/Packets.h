#pragma once
#include <cstdint>
#include "PacketID.h"
#include "ProtocolTypes.h"
#include "VarEnum.h"

#pragma pack(push, 1)

//////////// 패킷 ////////////////
struct PacketHeader
{
    uint16_t size;
    uint16_t id;
};

// ===== 로그인 =====
struct REQ_LoginPacket
{
    PacketHeader header;
    char userID[32];
    char password[32];
};

struct RES_LoginPacket
{
    PacketHeader header;
    uint64_t userUID;
    bool success;
    char message[50];
};

struct REQ_CreateCharacterPacket
{
    PacketHeader header;
    uint32_t charID;
    char name[50]; // UTF-8, 널 종료 보장 안 됨
};

struct RES_CreateCharacterPacket
{
    PacketHeader header;
    CreateCharacterResult result;
    CharacterSummary character; // Success일 때만 유효
};

// ===== 로비 =====
struct REQ_CharacterListPacket
{
    PacketHeader header;
};

struct RES_CharacterListPacket
{
    PacketHeader header;
    uint8_t count;
};

struct REQ_SelectCharacterPacket
{
    PacketHeader header;
    uint64_t playerID;
};

struct RES_SelectCharacterPacket
{
    PacketHeader header;
    uint32_t zoneID;
    bool success;
};

struct RES_EnterGamePacket
{ // 나 자신의 입장 결과
    PacketHeader header;
    uint32_t zoneID;
    uint32_t channelID;
    PlayerInfo me;
    uint64_t exp;
};

struct RES_SpawnPacket
{ // 가변 길이: header + count + PlayerInfo[count]
    PacketHeader header;
    uint16_t count;
};

struct RES_DespawnPacket
{
    PacketHeader header;
    uint64_t playerID;
};

struct REQ_ReadyToSpawnPacket
{
    PacketHeader header;
    uint64_t playerID; // Gateway가 덮어씀 (클라 값은 신뢰 안 함)
};

struct REQ_PortalPacket
{
    PacketHeader header;
    uint32_t portalID;
};

struct RES_ZoneChangePacket
{
    PacketHeader header;
    uint32_t zoneID;
    uint32_t channelID;
};

struct MovePacket
{
    PacketHeader header;
    uint64_t playerID;
    float x;
    float y;
    float z;
};

struct AttackPacket
{
    PacketHeader header;
    uint64_t playerID;
    int32_t targetId;
    int32_t damage;
};

struct RES_MonsterSpawnPacket
{
    PacketHeader header;
    uint16_t count;
};

struct RES_MonsterDespawnPacket
{
    PacketHeader header;
    uint64_t monsterUID;
};

struct RES_MonsterMovePacket
{
    PacketHeader header;
    uint64_t monsterUID;
    float x, y, z;
};

struct REQ_AttackPacket
{
    PacketHeader header;
    ObjectType targetType; // None = 논타겟 (바라보는 방향 전방 판정)
    uint64_t targetUID;    // 타겟 고유값 (논타겟이면 0)
    float dirX, dirZ;      // 바라보는 방향 (XZ 평면). 논타겟일 때 사용
};

struct RES_AttackPacket
{ // 공격 결과. 헛스윙이면 targetType = None
    PacketHeader header;
    ObjectType attackerType;
    uint64_t attackerUID;
    ObjectType targetType;
    uint64_t targetUID;
    int32_t damage;   // 실제로 깎인 양
    int32_t targetHp; // 맞은 뒤 남은 HP
    float dirX, dirZ; // 공격 방향 (주변 캐릭터 회전용)
};

struct RES_MonsterHpPacket
{
    PacketHeader header;
    uint64_t monsterUID;
    int32_t hp;
};

struct RES_ExpPacket
{
    PacketHeader header;
    uint64_t gainedExp; // 이번에 얻은 양
    uint64_t totalExp;  // 누적
};

struct REQ_RevivePacket
{
    PacketHeader header;
};

struct RES_RevivePacket
{
    PacketHeader header;
    uint64_t playerID;
    float x, y, z; // 부활 위치
    int32_t hp;
};

struct REQ_SkillCastPacket
{
    PacketHeader header;
    uint32_t skillID;
    ObjectType targetType;
    uint64_t targetUID;
    float dirX, dirZ;       // Cone / Line: 시전 방향
    float aimX, aimY, aimZ; // 이펙트 중심 (GroundArea 조준점 / TargetArea 대상 위치)
};

struct RES_SkillCastPacket
{ // 가변 길이: header + ... + SkillHit[hitCount]
    PacketHeader header;
    uint64_t casterID;
    uint32_t skillID;
    int32_t casterHp;       // 시전 후 남은 HP
    int32_t casterMp;       // 시전 후 남은 MP
    float dirX, dirZ;       // 시전 방향
    float aimX, aimY, aimZ; // 이펙트 중심 (GroundArea 조준점 / TargetArea 대상 위치)
    uint16_t hitCount;
};

struct RES_SkillFailPacket
{
    PacketHeader header;
    uint32_t skillID;
    SkillFailReason reason;
};

struct RES_PlayerStatPacket
{
    PacketHeader header;
    uint64_t playerID;
    int32_t hp;
    int32_t maxHp;
    int32_t mp;
    int32_t maxMp;
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

// 가변 길이 InventorySloatInfo
struct RES_InventoryPacket
{
    PacketHeader header;
    uint64_t gold;
    uint16_t count;
};

// 가변 길이 inventorySloatInfo
struct RES_InventoryUpdatePacket
{
    PacketHeader header;
    uint16_t count;
};

struct RES_GoldPacket
{
    PacketHeader header;
    int64_t delta;      // 변화량
    uint64_t totalGold; // 보유량
};

// packet + DropInfo[count]
struct RES_DropSpawnPacket
{
    PacketHeader header;
    uint16_t count;
};

struct RES_DropDespawnPacket
{
    PacketHeader header;
    uint64_t dropUID;
};

struct REQ_PickupPacket
{
    PacketHeader header;
    uint64_t dropUID;
};

struct RES_PickupFailPacket
{
    PacketHeader header;
    uint64_t dropUID;
    PickupFailReason reason;
};

struct REQ_UseItemPacket
{
    PacketHeader header;
    uint32_t itemID;
};

struct RES_UseItemFailPacket
{
    PacketHeader header;
    uint32_t itemID;
    UseItemFailReason reason;
};

struct REQ_DestroyItemPacket
{
    PacketHeader header;
    uint16_t slot;
    uint64_t itemUID;
    uint32_t count; // 몇 개 버릴지 (스택 일부만 가능)
};

// 내 스탯 정보
struct RES_MyStatPacket
{
    PacketHeader header;
    StatInfo stat;
};

// 장비 장착
struct REQ_EquipPacket
{
    PacketHeader header;
    uint64_t itemUID;
};

// 장비 해제
struct REQ_UnequipPacket
{
    PacketHeader header;
    EquipSlotType equipSlot;
};

// 장비 장착 실패
struct RES_EquipFailPacket
{
    PacketHeader header;
    EquipFailReason reason;
};

// 가변 길이: header + count + InventorySlotInfo[count]  (여기서 slot = EquipSlot 값, itemID 0 = 빈 칸)
struct RES_EquipmentPacket
{
    PacketHeader header;
    uint16_t count;
};

// 장비 변경
struct RES_EquipChangedPacket
{
    PacketHeader header;
    uint64_t playerID;
    EquipSlotType equipSlot;
    uint32_t itemID; // 0 = 해제
};

// 스킬 캐스팅 시작
struct RES_SkillCastStartPacket
{
    PacketHeader header;
    uint64_t casterID;      // 사용자 ID
    uint32_t skillID;       // 스킬 ID
    int32_t castingMs;      // 캐스팅 시간(ms)
    ObjectType targetType;  // 타겟 타입
    uint64_t targetUID;     // 타겟UID
    float dirX, dirZ;       // 시전 방향
    float aimX, aimY, aimZ; 
};

// 스킬 캐스팅 취소
struct RES_SkillCastCancelPacket
{
    PacketHeader header;
    uint64_t casterID;
    uint32_t skillID;
    CastCancelReason reason;
};

struct RES_BuffUpdatePacket
{
    PacketHeader header;
    StatType statType;
    ValueType valueType;
    int32_t value;
    int32_t remainMs;
};

// ==================== 파티 ====================
struct REQ_PartyInvitePacket
{
    PacketHeader header;
    uint64_t targetPlayerID;
};

struct RES_PartyInvitePacket
{ // 초대 받음
    PacketHeader header;
    uint64_t inviterID;
    char inviterName[50];
};

struct REQ_PartyAnswerPacket
{
    PacketHeader header;
    uint64_t inviterID;
    bool accept;
};

struct REQ_PartyLeavePacket
{
    PacketHeader header;
};

struct REQ_PartyKickPacket
{
    PacketHeader header;
    uint64_t targetPlayerID;
};

struct REQ_PartyChangeLeaderPacket
{
    PacketHeader header;
    uint64_t targetPlayerID;
};

struct RES_PartyInfoPacket
{ // 가변 길이: header + ... + PartyMemberInfo[count]. partyID == 0 이면 파티 없음
    PacketHeader header;
    uint64_t partyID;
    uint64_t leaderID;
    uint8_t count;
};

struct RES_PartyResultPacket
{
    PacketHeader header;
    PartyResult result;
};

#pragma pack(pop)