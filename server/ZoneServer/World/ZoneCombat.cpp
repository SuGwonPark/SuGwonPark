#include "pch.h"
#include "World/Zone.h"
#include "Object/Player.h"

#include "Object/Monster.h"
#include "Combat/Combat.h"
#include "World/ZoneConstants.h"
#include "Combat/HitArea.h"
#include "Util/MathUtil.h"

#include "Template/ZoneTemplate.h"
#include "Template/SkillTemplate.h"
#include "Template/SkillEffectTemplate.h"

#include "ServerCore/Manager/SendBufferManager.h"
#include "ServerCore/Protocol/Packets.h"

// Zone 전투 멤버 함수 (기본 공격, 스킬, 사망/부활)
// 전투 공식은 Combat.h, 상태 관리는 Player / Monster 쪽

// ==================== 기본 공격 ====================

void Zone::HandleAttack(const PlayerRef& attacker, ObjectType targetType, uint64_t targetUID, float dirX, float dirZ)
{
    if (attacker->IsDead())
        return;

    const uint64_t attackerUID = attacker->GetPlayerID();
    if (portalPending_.count(attackerUID))
        return;

    // 플레이어 공격 범위 + 지연 오차
    const float range = ZoneConst::PLAYER_ATTACK_RANGE + ZoneConst::ATTACK_RANGE_TOLERANCE;

    Monster* target = nullptr;
    switch (targetType)
    {
        case ObjectType::Monster: {
            auto it = monsters_.find(targetUID);
            if (it == monsters_.end())
                return;

            Monster& monster = *it->second;

            if (monster.IsDead() || monster.GetState() == MonsterState::Return)
                return;

            if (monster.DistSqTo(attacker->GetX(), attacker->GetY(), attacker->GetZ()) > range * range)
                return;

            target = &monster;
            break;
        }
        case ObjectType::None: {
            if (!Math::NormalizeXZ(dirX, dirZ))
                return;

            break;
        }
        default:
            return;
    }

    // 공격 쿨타임 체크
    const auto now = std::chrono::steady_clock::now();
    if (!attacker->TryUseAttack(now))
        return;

    // 대상 결정 : 논타겟은 전방 부채꼴에서 가까운 몬스터 체크
    if (!target)
    {
        std::vector<Monster*> found;
        CollectMonstersInArea(
            HitArea::Cone(attacker->GetX(), attacker->GetZ(), dirX, dirZ, range, ZoneConst::PLAYER_ATTACK_ANGLE), 1,
            found);
        if (!found.empty())
            target = found[0];
    }
    else
    {

        // 타겟 지정
        dirX = target->GetX() - attacker->GetX();
        dirZ = target->GetZ() - attacker->GetZ();
        if (!Math::NormalizeXZ(dirX, dirZ))
            dirX = dirZ = 0.f;
    }

    RES_AttackPacket pkt{};
    pkt.header.id = PKT_S_ATTACK;
    pkt.header.size = sizeof(pkt);
    pkt.attackerType = ObjectType::Player;
    pkt.attackerUID = attackerUID;
    pkt.dirX = dirX;
    pkt.dirZ = dirZ;

    // 타겟이 없을 경우 내 주위 유저에게 내 공격 모션만 보여줌
    if (!target)
    {
        pkt.targetType = ObjectType::None;
        BroadcastNear(CellOf(*attacker), &pkt, sizeof(pkt));
        return;
    }

    const int32_t raw = Combat::ApplyCritical(attacker->GetStat(), attacker->GetDamage());
    const int32_t damage = target->TakeDamage(Combat::ReduceByDefense(raw, target->GetStat()), attackerUID);

    if (target->GetState() == MonsterState::Idle)
        target->StartChase(attackerUID);

    pkt.targetType = ObjectType::Monster;
    pkt.targetUID = target->GetMonsterUID();
    pkt.damage = damage;
    pkt.targetHp = target->GetHp();
    BroadcastNear(CellOf(*target), &pkt, sizeof(pkt));

    if (target->GetHp() <= 0)
    {
        OnMonsterDead(*target);
    }
}

void Zone::MonsterAttack(Monster& monster, const PlayerRef& target)
{
    const CombatStat& ms = monster.GetStat();
    const int32_t raw = Combat::ApplyCritical(ms, ms.damage);
    const int32_t damage = target->TakeDamage(Combat::ReduceByDefense(raw, target->GetStat()));

    float dirX = target->GetX() - monster.GetX();
    float dirZ = target->GetZ() - monster.GetZ();
    if (!Math::NormalizeXZ(dirX, dirZ))
        dirX = dirZ = 0.f;

    RES_AttackPacket pkt{};
    pkt.header.id = PKT_S_ATTACK;
    pkt.header.size = sizeof(pkt);
    pkt.attackerType = ObjectType::Monster;
    pkt.attackerUID = monster.GetMonsterUID();
    pkt.targetType = ObjectType::Player;
    pkt.targetUID = target->GetPlayerID();
    pkt.damage = damage;
    pkt.targetHp = target->GetHp();
    pkt.dirX = dirX;
    pkt.dirZ = dirZ;
    BroadcastNear(CellOf(*target), &pkt, sizeof(pkt));
}

void Zone::BroadcastMonsterHp(const Monster& monster)
{
    RES_MonsterHpPacket pkt{};
    pkt.header.id = PKT_S_MONSTER_HP;
    pkt.header.size = sizeof(pkt);
    pkt.monsterUID = monster.GetMonsterUID();
    pkt.hp = monster.GetHp();
    BroadcastNear(CellOf(monster), &pkt, sizeof(pkt));
}

void Zone::OnMonsterDead(Monster& monster)
{
    const AoiCell cell = CellOf(monster);
    monster.Die(std::chrono::steady_clock::now());

    RES_MonsterDespawnPacket pkt{};
    pkt.header.id = PKT_S_MONSTER_DESPAWN;
    pkt.header.size = sizeof(pkt);
    pkt.monsterUID = monster.GetMonsterUID();
    BroadcastNear(cell, &pkt, sizeof(pkt));

    monsterGrid_.Remove(monster.GetMonsterUID(), cell);

    // 기여도 체크 및 보상 지급
    DistributeRewards(monster);
}

void Zone::CollectMonstersInArea(const HitArea& area, size_t maxCount, std::vector<Monster*>& out) const
{
    std::vector<std::pair<float, Monster*>> found;

    monsterGrid_.ForEachInRange(area.ox, area.oz, area.BoundRadius(), [&](uint64_t uid) {
        auto it = monsters_.find(uid);
        if (it == monsters_.end())
            return;

        Monster* m = it->second.get();
        if (m->IsDead() || m->GetState() == MonsterState::Return)
            return;
        if (!area.Contains(m->GetX(), m->GetZ()))
            return;

        const float dx = m->GetX() - area.ox, dz = m->GetZ() - area.oz;
        found.emplace_back(dx * dx + dz * dz, m);
    });

    // 가까운 순으로 앞쪽 n개 정렬
    const size_t n = std::min(found.size(), maxCount);
    std::partial_sort(
        found.begin(), found.begin() + n, found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    for (size_t i = 0; i < n; ++i)
        out.push_back(found[i].second);
}

// ==================== 스킬 ====================

void Zone::HandleSkillCast(const PlayerRef& caster, const REQ_SkillCastPacket& req)
{
    const uint32_t skillID = req.skillID;
    const uint64_t casterID = caster->GetPlayerID();

    float dirX = req.dirX, dirZ = req.dirZ;
    const bool hasDir = Math::NormalizeXZ(dirX, dirZ);
    float aimX = caster->GetX(), aimY = caster->GetY(), aimZ = caster->GetZ();

    if (portalPending_.count(casterID))
        return;

    auto fail = [&](SkillFailReason reason) { SendSkillFail(caster, skillID, reason); };

    if (caster->IsDead())
        return fail(SkillFailReason::Dead);

    const SkillData* skill = SkillTemplate::Instance().GetData(caster->GetClassType(), skillID);
    if (!skill)
        return fail(SkillFailReason::InvalidSkill);

    // 아군/본인 대상 스킬(회복, 버프 등)은 아직 구현 전: 몬스터에게 쓰이지 않도록 막아 둔다
    if (skill->targetType != TargetType::Enemy)
        return fail(SkillFailReason::InvalidSkill);

    // 스킬 효과 그룹
    const std::vector<SkillEffectData>* effects = SkillEffectTemplate::Instance().GetGroup(skill->effectGroupID);
    if (!effects)
        return fail(SkillFailReason::InvalidSkill);

    const auto now = std::chrono::steady_clock::now();
    // 스킬 쿨타임 확인
    if (!caster->IsSkillReady(skillID, now))
        return fail(SkillFailReason::Cooldown);
    // 스킬 코스트 확인
    if (!caster->CheckSkillCost(skill->costType, skill->cost))
        return fail(SkillFailReason::NotEnoughCost);

    // 지정 대상 1마리 (Single / TargetArea 공용): 타입 → 존재/상태 → 사거리 순으로 검사
    auto findTarget = [&](SkillFailReason& reason) -> Monster* {
        if (req.targetType != ObjectType::Monster)
        {
            reason = SkillFailReason::InvalidTarget;
            return nullptr;
        }

        auto it = monsters_.find(req.targetUID);
        if (it == monsters_.end() || it->second->IsDead() || it->second->GetState() == MonsterState::Return)
        {
            reason = SkillFailReason::InvalidTarget;
            return nullptr;
        }

        const float range = skill->range + ZoneConst::ATTACK_RANGE_TOLERANCE;
        if (it->second->DistSqTo(caster->GetX(), caster->GetY(), caster->GetZ()) > range * range)
        {
            reason = SkillFailReason::OutOfRange;
            return nullptr;
        }
        return it->second.get();
    };

    const float cx = caster->GetX(), cz = caster->GetZ();
    std::vector<Monster*> targets;
    SkillFailReason reason = SkillFailReason::None;
    Monster* target{};
    switch (skill->rangeType)
    {
        case RangeType::Single: {
            target = findTarget(reason);
            if (!target)
                return fail(reason);
            targets.push_back(target);
            break;
        }
        case RangeType::SelfArea: {
            CollectMonstersInArea(HitArea::Circle(cx, cz, skill->radius), skill->targetCount, targets);
            break;
        }
        case RangeType::TargetArea: {
            target = findTarget(reason);
            if (!target)
                return fail(reason);

            CollectMonstersInArea(
                HitArea::Circle(target->GetX(), target->GetZ(), skill->radius), skill->targetCount, targets);
            break;
        }
        case RangeType::Cone: {
            if (!hasDir)
                return fail(SkillFailReason::InvalidAim);
            CollectMonstersInArea(
                HitArea::Cone(cx, cz, dirX, dirZ, skill->range, skill->angle), skill->targetCount, targets);
            break;
        }
        case RangeType::Line: {
            if (!hasDir)
                return fail(SkillFailReason::InvalidAim);
            CollectMonstersInArea(
                HitArea::Line(cx, cz, dirX, dirZ, skill->range, skill->width), skill->targetCount, targets);
            break;
        }
        case RangeType::GroundArea: {
            if (!Math::IsFinite3(req.aimX, req.aimY, req.aimZ))
                return fail(SkillFailReason::InvalidAim);

            const float maxRange = skill->range + ZoneConst::ATTACK_RANGE_TOLERANCE;
            if (!Math::IsWithinRangeXZ(cx, cz, req.aimX, req.aimZ, maxRange))
                return fail(SkillFailReason::OutOfRange);

            aimX = req.aimX;
            aimY = req.aimY;
            aimZ = req.aimZ;
            CollectMonstersInArea(HitArea::Circle(aimX, aimZ, skill->radius), skill->targetCount, targets);
            break;
        }
        default:
            return fail(SkillFailReason::InvalidSkill);
    }

    // 타겟 지정 스킬은 방향과 이팩트 중심을 대상 기준
    if (target)
    {
        aimX = target->GetX();
        aimY = target->GetY();
        aimZ = target->GetZ();
        dirX = aimX - cx;
        dirZ = aimZ - cz;
        if (!Math::NormalizeXZ(dirX, dirZ))
            dirX = dirZ = 0.f;
    }
    else if (!hasDir)
    {
        dirX = dirZ = 0.f;
    }

    // 스킬 확정 이후 처리: 자원 처리 -> 데미지 -> 결과 -> 사망처리
    caster->TrySpendSkill(skill->costType, skill->cost);
    caster->StartSkillCooldown(skillID, now, skill->cooldownMs);

    std::vector<SkillHit> hits;
    hits.reserve(targets.size());
    for (Monster* m : targets)
    {
        SkillHit hit{};
        hit.targetType = ObjectType::Monster;
        hit.targetUID = m->GetMonsterUID();
        hit.damage = ApplySkillEffect(caster, *effects, *m, now);
        hit.targetHp = m->GetHp();

        hits.push_back(hit);
    }

    RES_SkillCastPacket pkt{};
    pkt.header.id = PKT_S_SKILL_CAST;
    pkt.casterID = casterID;
    pkt.skillID = skillID;
    pkt.casterHp = caster->GetHp();
    pkt.casterMp = caster->GetMp();
    pkt.dirX = dirX;
    pkt.dirZ = dirZ;
    pkt.aimX = aimX;
    pkt.aimY = aimY;
    pkt.aimZ = aimZ;
    pkt.hitCount = static_cast<uint16_t>(hits.size());

    // 타겟이 없으면 caster 기준으로 broadcast
    if (SendBufferRef buf = SendBufferManager::MakeList(pkt, hits))
        BroadcastNear(
            target ? CellOf(*target) : CellOf(*caster), buf->Buffer(), static_cast<uint16_t>(buf->AllocSize()));

    for (Monster* m : targets)
    {
        if (m->GetHp() <= 0)
            OnMonsterDead(*m);
    }
}

void Zone::SendSkillFail(const PlayerRef& caster, uint32_t skillID, SkillFailReason reason)
{
    RES_SkillFailPacket pkt{};
    pkt.header.id = PKT_S_SKILL_FAIL;
    pkt.header.size = sizeof(pkt);
    pkt.skillID = skillID;
    pkt.reason = reason;
    SendTo(caster, &pkt, sizeof(pkt));
}

int32_t Zone::ApplySkillEffect(
    const PlayerRef& caster,
    const std::vector<SkillEffectData>& effects,
    Monster& monster,
    std::chrono::steady_clock::time_point now)
{
    int32_t totalDamage = 0;
    for (const SkillEffectData& effect : effects)
    {
        switch (effect.effectType)
        {
            case SkillEffectType::Damage: {
                const int32_t raw =
                    Combat::ApplyCritical(caster->GetStat(), Combat::CalcSkillValue(caster->GetStat(), effect));
                totalDamage +=
                    monster.TakeDamage(Combat::ReduceByDefense(raw, monster.GetStat()), caster->GetPlayerID());
            }
            break;

            case SkillEffectType::Debuff: {
                monster.AddDebuff(
                    effect.statType, effect.valueType, effect.value,
                    now + std::chrono::milliseconds(effect.durationMs));
            }
            break;

            case SkillEffectType::DotDamage: {
                monster.AddDot(
                    caster->GetPlayerID(), effect.dotType, Combat::CalcSkillValue(caster->GetStat(), effect), now,
                    now + std::chrono::milliseconds(effect.durationMs));
            }
            break;
        }
    }

    // 몬스터가 idle 상태면 공격한 플레이어를 타깃으로 chase 시작
    if (monster.GetState() == MonsterState::Idle)
        monster.StartChase(caster->GetPlayerID());

    return totalDamage;
}

// 몬스터 도트 데미지 처리
bool Zone::UpdateMonsterEffects(Monster& monster, std::chrono::steady_clock::time_point now)
{
    // 몬스터 디버프 및 도트 업데이트 (반환 도트 총 데미지)
    const int32_t dotDamage = monster.UpdateEffects(now);

    if (dotDamage <= 0)
        return false;

    BroadcastMonsterHp(monster);

    if (monster.GetHp() > 0)
        return false;

    OnMonsterDead(monster);
    return true;
}

// ==================== 사망 / 부활 ====================

void Zone::HandleRevive(const PlayerRef& player)
{
    if (!player->IsDead())
        return;

    const ZoneData* zone = ZoneTemplate::Instance().GetData(zoneID_);
    if (!zone)
        return;

    const AoiCell from = CellOf(*player);
    player->Revive(zone->spawnX, zone->spawnY, zone->spawnZ, player->GetMaxHp());
    const AoiCell to = CellOf(*player);
    OnPlayerCellChanged(player, from, to);

    RevivePlayer(player);
}

void Zone::RevivePlayer(const PlayerRef& player)
{
    RES_RevivePacket pkt{};
    pkt.header.id = PKT_S_REVIVE;
    pkt.header.size = sizeof(pkt);
    pkt.playerID = player->GetPlayerID();
    pkt.x = player->GetX();
    pkt.y = player->GetY();
    pkt.z = player->GetZ();
    pkt.hp = player->GetHp();
    BroadcastNear(CellOf(*player), &pkt, sizeof(pkt));
    return;
}
