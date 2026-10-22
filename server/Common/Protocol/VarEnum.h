
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
	PickOne = 3,       // 여러 개 중 1개만 선택 (가중치)
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