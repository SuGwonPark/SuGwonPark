#include "pch.h"
#include "Template/RewardTemplate.h"
#include "ServerCore/Utils/CsvReader.h"
#include "ServerCore/Utils/util.h"       

namespace {
	// 수량을 MinCount ~ MaxCount에서 굴려 결과에 추가
	void Push(std::vector<RewardResult>& out, const RewardItemData& item) {
		const uint32_t count = static_cast<uint32_t>(
			Random::Int(static_cast<int>(item.minCount), static_cast<int>(item.maxCount)));
		out.push_back({ item.delivery, item.type, item.value, count });
	}
}

bool RewardTemplate::Load()
{
	groups_.clear();
	itemGroups_.clear();
	rewardRows_ = itemRows_ = 0;

	std::unordered_set<uint32_t> rewardIDs;
	bool ok = Csv::ForEachRow("Reward.csv", [&](const CsvRow& r) {
		RewardData d{};
		d.id = Csv::ToU32(r.at(0));
		d.groupID = Csv::ToU32(r.at(1));
		d.type = ParseCsvEnum<RewardType>(r.at(2), "RewardType");
		d.itemGroupID = Csv::ToU32(r.at(3));

		if (d.groupID == 0) throw std::invalid_argument("RewardGroupID는 1 이상 (0은 '보상 없음' 예약)");
		if (!rewardIDs.insert(d.id).second) throw std::invalid_argument("중복 ID " + r.at(0));
		groups_[d.groupID].push_back(d);
		++rewardRows_;
		});
	if (!ok) return false;

	std::unordered_set<uint32_t> itemIDs;
	ok = Csv::ForEachRow("RewardItem.csv", [&](const CsvRow& r) {
		RewardItemData d{};
		d.id = Csv::ToU32(r.at(0));
		d.groupID = Csv::ToU32(r.at(1));
		d.delivery = ParseCsvEnum<RewardDeliveryType>(r.at(2), "DeliveryType");
		d.type = ParseCsvEnum<RewardItemType>(r.at(3), "Type");
		d.value = Csv::ToU32(r.at(4));
		d.minCount = Csv::ToU32(r.at(5));
		d.maxCount = Csv::ToU32(r.at(6));
		d.rate = Csv::ToU32(r.at(7));

		if (d.minCount > d.maxCount)  throw std::invalid_argument("MinCount가 MaxCount보다 큼");
		if (d.maxCount == 0)          throw std::invalid_argument("MaxCount는 1 이상");
		if (d.rate > RewardMaxRate)   throw std::invalid_argument("Rate는 0 ~ " + std::to_string(RewardMaxRate));
		if (d.type != RewardItemType::Exp && d.value == 0) throw std::invalid_argument("Asset/Item은 Value가 필요");
		if (!itemIDs.insert(d.id).second) throw std::invalid_argument("중복 ID " + r.at(0));

		RewardItemGroup& group = itemGroups_[d.groupID];

		// ItemGroupData 세팅
		group.items.push_back(d);
		group.pickWeights.push_back(static_cast<int>(d.rate));
		group.rateSum += d.rate;
		++itemRows_;
		});
	if (!ok) return false;

	// Random용 가중치: 아이템 rate들 + 맨 뒤에 "꽝" 칸 (RewardMaxRate - 합)
	for (auto& [groupID, group] : itemGroups_) {
		group.randomWeights = group.pickWeights;
		const uint64_t miss = group.rateSum < RewardMaxRate ? RewardMaxRate - group.rateSum : 0;
		// rateSum이 100만 미만인 경우 나머진 꽝으로 대체
		group.randomWeights.push_back(static_cast<int>(miss));
	}
	return true;
}

bool RewardTemplate::Validate() const
{
	bool ok = true;
	for (const auto& [groupID, rewards] : groups_) {
		for (const auto& reward : rewards) {
			auto it = itemGroups_.find(reward.itemGroupID);
			if (it == itemGroups_.end()) {
				std::cerr << "[Template] Reward.csv ID " << reward.id << ": RewardItem.csv에 없는 RewardItemGroup " << reward.itemGroupID << std::endl;
				ok = false;
				continue;
			}

			const uint64_t sum = it->second.rateSum;
			if (reward.type == RewardType::Random && sum > RewardMaxRate) {
				std::cerr << "[Template] Reward.csv ID " << reward.id << ": Random인데 그룹 " << reward.itemGroupID
					<< "의 Rate 합이 " << RewardMaxRate << " 초과 (" << sum << ")" << std::endl;
				ok = false;
			}
		}
	}
	return ok;
}

void RewardTemplate::Roll(uint32_t groupID, std::vector<RewardResult>& out) const
{
	auto it = groups_.find(groupID);
	if (it == groups_.end()) return;

	for (const auto& reward : it->second)
		RollItemGroup(reward, out);
}

void RewardTemplate::RollItemGroup(const RewardData& reward, std::vector<RewardResult>& out) const
{
	auto it = itemGroups_.find(reward.itemGroupID);
	if (it == itemGroups_.end()) return;
	const RewardItemGroup& group = it->second;

	switch (reward.type) {
	case RewardType::All:              // 전부 지급 (Rate 무시)
		for (const auto& item : group.items) Push(out, item);
		break;

	case RewardType::Independent:      // 아이템마다 rate / RewardMaxRate 확률
		for (const auto& item : group.items)
			if (Random::ChanceRate(static_cast<int>(item.rate), static_cast<int>(RewardMaxRate))) Push(out, item);
		break;

	case RewardType::Random:
	{         // RewardMaxRate 기준 1개, 맨 뒤 칸에 걸리면 꽝
		const int index = Random::WeightedIndex(group.randomWeights);
		if (index >= 0 && index < static_cast<int>(group.items.size())) Push(out, group.items[index]);
		break;
	}

	case RewardType::PickOne: // 나중에 선택형 나오면 생각해봄
	case RewardType::None:             // 로드 단계에서 걸러져 여기 오지 않음
		break;
	}
}