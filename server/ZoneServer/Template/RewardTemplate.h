#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "ServerCore/Protocol/VarEnum.h"   

struct RewardData {
	uint32_t   id;				// RewardID
	uint32_t   groupID;         // RewardGroupID
	RewardType type;			// 보상 지급 방식
	uint32_t   itemGroupID;		// 지급 아이템 그룹
};

struct RewardItemData {
	uint32_t           id;
	uint32_t           groupID;		// 지급 아이템 그룹
	RewardDeliveryType delivery;	// 전달 방식
	RewardItemType     type;		// 지급 아이템 타입 (골드, exp, 경험치, 아이템 등)
	uint32_t           value;		// 세부 타입
	uint32_t           minCount;	// 최소 수량
	uint32_t           maxCount;	// 최대 수량
	uint32_t           rate;		// 확률 (일부 Delivery 타입에선 체크 안함)
};

struct RewardItemGroup {               // RewardItem.csv의 GroupID 하나
	std::vector<RewardItemData> items;
	std::vector<int> pickWeights;      // PickOne용: items의 rate 그대로
	std::vector<int> randomWeights;    // Random용: items의 rate + 맨 뒤에 꽝(RewardMaxRate - 합)
	uint64_t rateSum = 0;
};

struct RewardResult {				// 보상 획득 결과
	RewardDeliveryType delivery;
	RewardItemType     type;
	uint32_t           value;
	uint32_t           count;
};

class RewardTemplate {
public:
	static RewardTemplate& Instance() { static RewardTemplate inst; return inst; }

	bool Load();                       // Reward.csv + RewardItem.csv
	bool Validate() const;             // Reward → RewardItem 참조, 그룹별 Rate 합

	bool HasGroup(uint32_t groupID) const { return groups_.count(groupID) > 0; }

	// 보상 그룹을 굴려 결과를 out에 추가 (난수는 util의 Random:: 스레드별 엔진 사용)
	void Roll(uint32_t groupID, std::vector<RewardResult>& out) const;

	size_t RewardRowCount() const { return rewardRows_; }
	size_t ItemRowCount() const { return itemRows_; }

private:
	void RollItemGroup(const RewardData& reward, std::vector<RewardResult>& out) const;

	std::unordered_map<uint32_t, std::vector<RewardData>> groups_;      // RewardGroupID 
	std::unordered_map<uint32_t, RewardItemGroup>         itemGroups_;  // RewardItem GroupID 
	size_t rewardRows_ = 0;
	size_t itemRows_ = 0;
};