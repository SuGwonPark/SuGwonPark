#pragma once
#include <cstdint>

inline constexpr uint32_t RewardMaxRate = 1000000;

inline constexpr uint32_t BuffMaxPercent = 10000;

enum class ObjectType : uint8_t
{
    Player = 1,
    Monster = 2,
};

// 보상 지급 방식
enum class RewardDeliveryType : uint8_t
{
    None = 0,
    Direct = 1, // 바로 획득 (인벤토리에 즉시 지급)
    Drop = 2,   // 필드에 드랍 (줍기 필요)
    Dice = 3,   // 주사위 (파티원 굴려서 분배)
    Mail = 4,   // 우편 지급 (인벤토리 가득 찼을 때 등)
};

// 보상 타입
enum class RewardType : uint8_t
{
    None = 0,
    All = 1,         // 전부 지급
    Independent = 2, // 각 행을 개별 확률로 굴림
    PickOne = 3,     // 여러 개 중 1개만 선택
    Random = 4,      // 랜덤으로 1개만 지급
};

// 보상 아이템 타입
enum class RewardItemType : uint8_t
{
    None = 0,
    Exp = 1,
    Asset = 2,
    Item = 3,
};

// 스킬 효과 타입
enum class SkillEffectType : uint8_t
{
    None = 0,
    Damage = 1,    // 데미지
    HealHp = 2,    // hp 회복
    HealMp = 3,    // mp 회복
    Buff = 4,      // 버프
    Debuff = 5,    // 디버프
    Revive = 6,    // 부활
    DotDamage = 7, // 지속 데미지
};

// 도트 스킬 타입
enum class DotType : uint8_t
{
    None = 0,
    Poison = 1, // 독
    Burn = 2,   // 화상
    Bleed = 3,  // 출혈
};

// 타겟 타입
enum class TargetType : uint8_t
{
    None = 0,
    Enemy = 1, // 상대, 몬스터
    Ally = 2,  // 파티
    Self = 3,  // 본인
};

// 범위 타입
enum class RangeType : uint8_t
{
    None = 0,
    Single = 1,     // 대상 1명 (Range 안)
    SelfArea = 2,   // 시전자 중심 원형 (Radius)
    TargetArea = 3, // 대상 위치 중심 원형 (Range 안에서, Radius)
};

enum class SkillCostType : uint8_t
{
    None = 0,
    Hp = 1,
    Mp = 2,
};

enum class ClassType : uint8_t
{
    None = 0,
    Warrior = 1,
    Archer = 2,
    Mage = 3,
    Priest = 4,
};

// 캐릭터 생성 상태값
enum class CreateCharacterResult : uint8_t
{
    None = 0,
    Success = 1,
    InvalidName = 2,   // 길이 / 글자 규칙 위반
    DuplicateName = 3, // 이미 있는 이름
    SlotFull = 4,      // 슬롯 가득 참
    ServerError = 5,
    InvalidClass = 6, // 없는 직업
};

enum class ItemType : uint8_t
{
    None = 0,
    Equip = 1,
    Material = 2,   // 재료
    Consumable = 3, // 소모품
};

enum class EquipSlotType : uint8_t
{
    None = 0,
    Weapon = 1,   // 무기
    Head = 2,     // 모자
    Armor = 3,    // 갑옷
    Gloves = 4,   // 장갑
    Boots = 5,    // 신발
    Ring = 6,     // 반지
    Necklace = 7, // 목걸이
    Max = 8,      // 마지막
};

enum class GradeType : uint8_t
{
    None = 0,
    Normal = 1,
    Rare = 2,
    Unique = 3,
    Legendary = 4,
};

enum class ValueType : uint8_t
{
    None = 0,
    Flat = 1,    // 정수
    Percent = 2, // 퍼센티지
};

enum class StatType : uint8_t
{
    None = 0,
    Hp = 1,          // 최대 hp
    Mp = 2,          // 최대 mp
    Damage = 3,      // 공격력
    Defense = 4,     // 방어력
    CritRate = 5,    // 크확
    CritDamage = 6,  // 크뎀
    MoveSpeed = 7,   // 이동 속도
    AttackSpeed = 8, // 공격 속도
    HpRegen = 9,     // hp 리젠
    MpRegen = 10,    // mp 리젠
    Max = 11,   // 마지막
};

enum class GenderType : uint8_t
{
    None = 0,
    Male = 1,
    Female = 2,
};

enum class AssetType : uint8_t
{
    None = 0,
    Gold = 1,
};


// ==================== Fail Enum =============================
// 스킬 실패 (시전자에게만 전송)
enum class SkillFailReason : uint8_t
{
    None = 0,
    InvalidSkill = 1,  // 없는 스킬
    Cooldown = 2,      // 쿨타임 중
    NotEnoughCost = 3, // MP 부족
    InvalidTarget = 4, // 대상 없음 / 죽음 / 귀환 중
    OutOfRange = 5,    // 사거리 밖
    Dead = 6,          // 시전자가 사망 상태
};

// 줍기 실패 (주운 사람에게만)
enum class PickupFailReason : uint8_t
{
    None = 0,
    NotFound = 1,      // 없음 (이미 누가 주웠거나 사라짐)
    TooFar = 2,        // 거리 밖
    NotOwner = 3,      // 우선권 시간 중 다른 사람 것
    InventoryFull = 4, // 인벤토리에 다 안 들어감
    Dead = 5,          // 죽은 상태
};


// 아이템 사용 실패 (사용한 사람에게만)
enum class UseItemFailReason : uint8_t
{
    None = 0,
    InvalidItem = 1, // 빈 칸 / itemUID 불일치 / 소모품 아님
    Cooldown = 2,
    LevelTooLow = 3,
    WrongClass = 4,
    Dead = 5,      // 죽은 상태에서 회복 아이템
    NotDead = 6,   // 살아 있는데 부활 아이템
    NotUsable = 7, // 아직 처리하지 않는 효과 (Buff 등)
    NotEffect = 8,
};

// 장비 착용/해제 실패
enum class EquipFailReason : uint8_t
{
    None = 0,
    InvalidItem = 1, // 없는 itemUID / 잘못된 장착 칸
    NotEquip = 2,    // 장비가 아님
    LevelTooLow = 3,
    WrongClass = 4,
    Dead = 5,
    InventoryFull = 6, // 해제할 빈 칸 없음
    EmptySlot = 7,     // 해제하려는 칸이 비어 있음
};