#pragma once
#include <cstdint>
#include <unordered_map>
#include "ServerCore/Protocol/VarEnum.h" 

struct StatData
{
    ClassType classType;
    uint16_t level;
    int32_t maxHp;
    int32_t maxMp;
    int32_t damage;
};

class StatTemplate
{
  public:
    static StatTemplate& Instance()
    {
        static StatTemplate inst;
        return inst;
    }

    bool Load();
    bool Validate() const;                                          // 모든 직업이 1 ~ 최대 레벨(Level.csv)까지 있는지
    const StatData* Get(ClassType classType, uint16_t level) const; // 없으면 nullptr

  private:
    static uint32_t Key(ClassType classType, uint16_t level)
    {
        // classType bit가 상위 16bit, level bit가 하위 16bit
        return (static_cast<uint32_t>(classType) << 16) | level;
    }

    std::unordered_map<uint32_t, StatData> data_;
};