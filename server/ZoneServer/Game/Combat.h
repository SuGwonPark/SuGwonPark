#pragma once
#include <vector>
#include "Game/CombatStat.h"

struct SkillEffectData;

// 전투 공식 모음 (상태 없음). Player / Monster 공용
//  일반 스탯(HP, MP, 공격력, 방어력, 이동속도, 리젠): (기본 + Flat 합) × (1 + Percent 합)
//  확률/배율 스탯(치명타, 공격속도): Flat/Percent 구분 없이 만분율 포인트로 더함
namespace Combat
{
// 기본값 + 수정치(장비, 버프, 디버프) → 최종 스탯
CombatStat CalcStat(const CombatStat& base, const std::vector<StatModifier>& mods);

// 치명타: critRate 확률로 critDamage 배율
int32_t ApplyCritical(const CombatStat& attacker, int32_t damage);
// 방어력 적용: 받는 비율 100 / (100 + 방어력), 최소 1
int32_t ReduceByDefense(int32_t damage, const CombatStat& defender);
// 공격속도를 반영한 쿨타임
int32_t AttackCooldownMs(int32_t baseMs, const CombatStat& attacker);

// 스킬 수치: Flat = 값 그대로, Percent = 시전자 공격력의 만분율
int32_t CalcSkillValue(const CombatStat& caster, const SkillEffectData& e);
// 회복량: Flat = 값 그대로, Percent = 최대치의 만분율
int32_t CalcRecoverAmount(const SkillEffectData& e, int32_t maxValue);
} // namespace Combat
