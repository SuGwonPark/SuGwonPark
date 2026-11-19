#pragma once
#include <chrono>
#include <cstdint>
#include <vector>
#include "Combat/CombatStat.h"
#include "ServerCore/Protocol/VarEnum.h"

// 지속 효과 추가 결과
enum class EffectResult : uint8_t
{
    Ignored,   // 기존보다 약해서 무시
    Refreshed, // 같은 수치: 시간만 갱신 (스탯 변화 없음)
    Applied,   // 새로 추가 / 더 강한 값으로 교체
};

// 이번 틱에 들어갈 도트 데미지 (시전자별)
struct DotHit
{
    uint64_t casterID;
    int32_t damage;
};

// 지속 효과 묶음 (버프 / 디버프 / 도트). Player, Monster 공용
class StatusEffects
{
  public:
    using Clock = std::chrono::steady_clock;

    // 스탯 효과 추가. value: 버프 +, 디버프 -
    // 같은 스탯·같은 타입·같은 방향은 중첩하지 않음: 더 강하면 교체, 같으면 시간 갱신, 약하면 무시
    EffectResult AddStat(StatType stat, ValueType valueType, int32_t value, Clock::time_point endAt);

    // 도트 추가. 같은 시전자의 같은 DotType은 위와 같은 규칙 (틱 주기는 유지)
    EffectResult AddDot(
        uint64_t casterID, DotType dotType, int32_t damagePerTick, Clock::time_point now, Clock::time_point endAt);

    // 끝난 효과 정리 + 이번에 들어갈 도트 수집
    // 끝난 스탯 효과는 expired, 도트는 dotHits에 담김. 스탯 효과가 하나라도 끝났으면 true (재계산 필요)
    bool Update(Clock::time_point now, std::vector<StatModifier>& expired, std::vector<DotHit>& dotHits);

    // 전부 제거 (사망 / 리스폰 / 귀환). 스탯 효과가 있었으면 true (removed가 있으면 거기에 담김)
    bool Clear(std::vector<StatModifier>* removed = nullptr);

    // 스탯 계산에 넣을 수정치
    void CollectModifiers(std::vector<StatModifier>& out) const;

    bool Empty() const
    {
        return stats_.empty() && dots_.empty();
    }

  private:
    struct StatEffect
    {
        StatModifier mod; // 디버프는 value 음수
        Clock::time_point endAt;
    };

    struct Dot
    {
        uint64_t casterID;
        DotType dotType;
        int32_t damagePerTick;
        Clock::time_point nextTickAt;
        Clock::time_point endAt;
    };

    std::vector<StatEffect> stats_;
    std::vector<Dot> dots_;
};