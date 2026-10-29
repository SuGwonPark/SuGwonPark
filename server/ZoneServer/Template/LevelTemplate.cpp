#include "pch.h"
#include "Template/LevelTemplate.h"
#include "ServerCore/Utils/CsvReader.h"

bool LevelTemplate::Load()
{
    requiredExp_.clear();
    return Csv::ForEachRow("Level.csv", [this](const CsvRow& r) {
        const uint32_t level = Csv::ToU32(r.at(1));
        const uint64_t exp = Csv::ToU64(r.at(2));

        // 1레벨부터 빠짐없이 순서대로, 누적 경험치는 계속 커져야 한다
        if (level != requiredExp_.size() + 1)
            throw std::invalid_argument(
                "Level은 1부터 순서대로 (기대값 " + std::to_string(requiredExp_.size() + 1) + ")");
        if (level == 1 && exp != 0)
            throw std::invalid_argument("1레벨 Exp는 0");
        if (!requiredExp_.empty() && exp <= requiredExp_.back())
            throw std::invalid_argument("Exp는 이전 레벨보다 커야 함");

        requiredExp_.push_back(exp);
    });
}

uint16_t LevelTemplate::LevelFromExp(uint64_t exp) const
{
    // exp 이하인 누적 경험치 개수 = 레벨  (예: [0, 50, 200]에서 exp 50 → 2레벨)
    const auto it = std::upper_bound(requiredExp_.begin(), requiredExp_.end(), exp);
    const auto level = it - requiredExp_.begin();
    return static_cast<uint16_t>(level < 1 ? 1 : level);
}