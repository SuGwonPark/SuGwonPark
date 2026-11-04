#pragma once
#include <cstdint>
#include <unordered_map>
#include "ServerCore/Protocol/VarEnum.h"

struct CharData
{
    uint8_t charID = 0;
    ClassType classType = ClassType::None;
    GenderType gender = GenderType::None;
};

class CharacterTemplate
{
  public:
    static CharacterTemplate& Instance()
    {
        static CharacterTemplate inst;
        return inst;
    }

    bool Load();
    const CharData* GetData(uint32_t charID) const; 

    const std::unordered_map<uint32_t, CharData>& All() const
    {
        return data_;
    }

  private:
    std::unordered_map<uint32_t, CharData> data_;
};
