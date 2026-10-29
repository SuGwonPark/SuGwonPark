#pragma once
#include <cstdint>
#include <vector>

// Data/Level.csv : ID,Level,Exp  (Exp = 그 레벨이 되기 위한 "누적" 경험치)
class LevelTemplate
{
  public:
    static LevelTemplate& Instance()
    {
        static LevelTemplate inst;
        return inst;
    }

    bool Load();

    // 누적 경험치 → 레벨 (1 ~ MaxLevel). 최대 레벨 이후 경험치는 계속 쌓이기만 한다
    uint16_t LevelFromExp(uint64_t exp) const;
    uint16_t MaxLevel() const
    {
        return static_cast<uint16_t>(requiredExp_.size());
    }

  private:
    // requiredExp_[i] = (i+1)레벨이 되기 위한 누적 경험치. [0]은 항상 0
    std::vector<uint64_t> requiredExp_;
};