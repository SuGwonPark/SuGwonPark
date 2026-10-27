#pragma once
#include <cstdint>


inline constexpr uint32_t RewardMaxRate = 1000000;



enum class ObjectType : uint8_t {
	Player = 1,
	Monster = 2,
};


// 보상 지급 방식
enum class RewardDeliveryType : uint8_t {
	None = 0,
	Direct = 1,   // 바로 획득 (인벤토리에 즉시 지급)
	Drop = 2,   // 필드에 드랍 (줍기 필요)
	Dice = 3,   // 주사위 (파티원 굴려서 분배)
	Mail = 4,   // 우편 지급 (인벤토리 가득 찼을 때 등)
};

// 보상 타입
enum class RewardType : uint8_t {
	None = 0,
	All = 1,           // 전부 지급
	Independent = 2,   // 각 행을 개별 확률로 굴림
	PickOne = 3,       // 여러 개 중 1개만 선택 
	Random = 4,			// 랜덤으로 1개만 지급
};

// 보상 아이템 타입
enum class RewardItemType : uint8_t
{
	None = 0,
	Exp = 1,
	Asset = 2,
	Item = 3,
};

// 스킬 대상 방식
enum class SkillTargetType : uint8_t {
	None = 0,
	Single = 1,      // 대상 1명 (Range 안)
	SelfArea = 2,    // 시전자 중심 원형 (Radius)
	TargetArea = 3,  // 대상 위치 중심 원형 (Range 안에서, Radius)
};

// 스킬 실패 이유 (시전자에게만 전송)
enum class SkillFailReason : uint8_t {
	None = 0,
	InvalidSkill = 1,   // 없는 스킬
	Cooldown = 2,       // 쿨타임 중
	NotEnoughCost = 3,    // MP 부족
	InvalidTarget = 4,  // 대상 없음 / 죽음 / 귀환 중
	OutOfRange = 5,     // 사거리 밖
	Dead = 6,           // 시전자가 사망 상태

};

enum class SkillCostType : uint8_t {
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
};

