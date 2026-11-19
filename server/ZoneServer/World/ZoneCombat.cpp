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

    if (attacker->IsCasting())
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

    if (target->IsDead())
        OnPlayerDead(target);
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
    if (portalPending_.count(caster->GetPlayerID()))
        return;

    auto fail = [&](SkillFailReason reason) { SendSkillFail(caster, skillID, reason); };

    if (caster->IsDead())
        return fail(SkillFailReason::Dead);
    if (caster->IsCasting())
        return fail(SkillFailReason::Casting);

    const SkillData* skill = SkillTemplate::Instance().GetData(caster->GetClassType(), skillID);
    if (!skill)
        return fail(SkillFailReason::InvalidSkill);

    // 아군/본인 대상 스킬(회복, 버프 등)은 아직 구현 전
    if (skill->targetType != TargetType::Enemy)
        return fail(SkillFailReason::InvalidSkill);

    const std::vector<SkillEffectData>* effects = SkillEffectTemplate::Instance().GetGroup(skill->effectGroupID);
    if (!effects)
        return fail(SkillFailReason::InvalidSkill);

    const auto now = std::chrono::steady_clock::now();
    if (!caster->IsSkillReady(skillID, now))
        return fail(SkillFailReason::Cooldown);
    if (!caster->CheckSkillCost(skill->costType, skill->cost))
        return fail(SkillFailReason::NotEnoughCost);

    // 대상 / 범위 판정 (시전 스킬도 시작 시점에 한 번 검사해서 바로 실패를 알려줌)
    SkillResolve res;
    SkillFailReason reason = SkillFailReason::None;
    if (!ResolveSkillTargets(caster, *skill, req, res, reason))
        return fail(reason);

    // 즉시 발동
    if (skill->castingMs <= 0)
        return ExecuteSkill(caster, *skill, *effects, res, now);

    // 시전 시작: 자원/쿨타임은 완료 시점에 소모
    caster->StartCasting(req, now + std::chrono::milliseconds(skill->castingMs));
    castingPlayers_.insert(caster->GetPlayerID());

    RES_SkillCastStartPacket pkt{};
    pkt.header.id = PKT_S_SKILL_CAST_START;
    pkt.header.size = sizeof(pkt);
    pkt.casterID = caster->GetPlayerID();
    pkt.skillID = skillID;
    pkt.castingMs = skill->castingMs;
    pkt.targetType = res.mainTarget ? ObjectType::Monster : ObjectType::None;
    pkt.targetUID = res.mainTarget ? res.mainTarget->GetMonsterUID() : 0;
    pkt.dirX = res.dirX;
    pkt.dirZ = res.dirZ;
    pkt.aimX = res.aimX;
    pkt.aimY = res.aimY;
    pkt.aimZ = res.aimZ;
    BroadcastNear(CellOf(*caster), &pkt, sizeof(pkt));
}

bool Zone::ResolveSkillTargets(
    const PlayerRef& caster,
    const SkillData& skill,
    const REQ_SkillCastPacket& req,
    SkillResolve& out,
    SkillFailReason& reason) const
{
    const float cx = caster->GetX(), cz = caster->GetZ();

    float dirX = req.dirX, dirZ = req.dirZ;
    const bool hasDir = Math::NormalizeXZ(dirX, dirZ);
    out.aimX = cx;
    out.aimY = caster->GetY();
    out.aimZ = cz;

    // 지정 대상 1마리 (Single / TargetArea 공용): 타입 → 존재/상태 → 사거리
    auto findTarget = [&]() -> Monster* {
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

        const float range = skill.range + ZoneConst::ATTACK_RANGE_TOLERANCE;
        if (it->second->DistSqTo(caster->GetX(), caster->GetY(), caster->GetZ()) > range * range)
        {
            reason = SkillFailReason::OutOfRange;
            return nullptr;
        }
        return it->second.get();
    };

    switch (skill.rangeType)
    {
        case RangeType::Single: {
            out.mainTarget = findTarget();
            if (!out.mainTarget)
                return false;
            out.targets.push_back(out.mainTarget);
            break;
        }
        case RangeType::SelfArea: {
            CollectMonstersInArea(HitArea::Circle(cx, cz, skill.radius), skill.targetCount, out.targets);
            break;
        }
        case RangeType::TargetArea: {
            out.mainTarget = findTarget();
            if (!out.mainTarget)
                return false;
            CollectMonstersInArea(
                HitArea::Circle(out.mainTarget->GetX(), out.mainTarget->GetZ(), skill.radius), skill.targetCount,
                out.targets);
            break;
        }
        case RangeType::Cone: {
            if (!hasDir)
            {
                reason = SkillFailReason::InvalidAim;
                return false;
            }

            CollectMonstersInArea(
                HitArea::Cone(cx, cz, dirX, dirZ, skill.range, skill.angle), skill.targetCount, out.targets);
            break;
        }
        case RangeType::Line: {
            if (!hasDir)
            {
                reason = SkillFailReason::InvalidAim;
                return false;
            }
            CollectMonstersInArea(
                HitArea::Line(cx, cz, dirX, dirZ, skill.range, skill.width), skill.targetCount, out.targets);
            break;
        }
        case RangeType::GroundArea: {
            if (!Math::IsFinite3(req.aimX, req.aimY, req.aimZ))
            {
                reason = SkillFailReason::InvalidAim;
                return false;
            }
            const float maxRange = skill.range + ZoneConst::ATTACK_RANGE_TOLERANCE;
            if (!Math::IsWithinRangeXZ(cx, cz, req.aimX, req.aimZ, maxRange))
            {
                reason = SkillFailReason::OutOfRange;
                return false;
            }
            out.aimX = req.aimX;
            out.aimY = req.aimY;
            out.aimZ = req.aimZ;
            CollectMonstersInArea(HitArea::Circle(out.aimX, out.aimZ, skill.radius), skill.targetCount, out.targets);
            break;
        }
        default:
            reason = SkillFailReason::InvalidSkill;
            return false;
    }

    // 타겟 지정 스킬은 방향과 이펙트 중심을 대상 기준으로
    if (out.mainTarget)
    {
        out.aimX = out.mainTarget->GetX();
        out.aimY = out.mainTarget->GetY();
        out.aimZ = out.mainTarget->GetZ();
        dirX = out.aimX - cx;
        dirZ = out.aimZ - cz;
        if (!Math::NormalizeXZ(dirX, dirZ))
            dirX = dirZ = 0.f;
    }
    else if (!hasDir)
        dirX = dirZ = 0.f;

    out.dirX = dirX;
    out.dirZ = dirZ;
    return true;
}

// 스킬 확정 : cost 감소 -> 데미지 -> 결과 -> 사망
void Zone::ExecuteSkill(
    const PlayerRef& caster,
    const SkillData& skill,
    const std::vector<SkillEffectData>& effects,
    const SkillResolve& res,
    std::chrono::steady_clock::time_point now)
{
    // cost 체크 및 감소
    caster->TrySpendSkill(skill.costType, skill.cost);
    // 스킬 쿨타임 적용
    caster->StartSkillCooldown(skill.skillID, now, skill.cooldownMs);

    // 몬스터 이펙트 적용 (데미지, 디버프, 도트)
    std::vector<SkillHit> hits;
    hits.reserve(res.targets.size());
    for (Monster* m : res.targets)
    {
        SkillHit hit{};
        hit.targetType = ObjectType::Monster;
        hit.targetUID = m->GetMonsterUID();
        hit.damage = ApplySkillEffect(caster, effects, *m, now);
        hit.targetHp = m->GetHp();
        hits.push_back(hit);
    }

    // 패킷 전송
    RES_SkillCastPacket pkt{};
    pkt.header.id = PKT_S_SKILL_CAST;
    pkt.casterID = caster->GetPlayerID();
    pkt.skillID = skill.skillID;
    pkt.casterHp = caster->GetHp();
    pkt.casterMp = caster->GetMp();
    pkt.dirX = res.dirX;
    pkt.dirZ = res.dirZ;
    pkt.aimX = res.aimX;
    pkt.aimY = res.aimY;
    pkt.aimZ = res.aimZ;
    pkt.hitCount = static_cast<uint16_t>(hits.size());

    if (SendBufferRef buf = SendBufferManager::MakeList(pkt, hits))
    {
        // 타겟이 있으면 타겟으로 없으면 시전자 기준
        AoiCell center = res.mainTarget ? CellOf(*res.mainTarget) : CellOf(*caster);
        BroadcastNear(center, buf->Buffer(), static_cast<uint16_t>(buf->AllocSize()));
    }

    // 몬스터 사망 처리
    for (Monster* m : res.targets)
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

// 캐스팅 시간 체크
void Zone::UpdateCasting(std::chrono::steady_clock::time_point now)
{
    if (castingPlayers_.empty())
        return;

    std::vector<PlayerRef> done;
    for (uint64_t playerID : castingPlayers_)
    {
        auto it = players_.find(playerID);
        if (it == players_.end())
            continue;

        const CastingInfo* c = it->second->GetCasting();
        if (c && now >= c->endAt)
            done.push_back(it->second);
    }

    for (const PlayerRef& p : done)
        CompleteCasting(p, now);
}

// 캐스팅 완료
void Zone::CompleteCasting(const PlayerRef& caster, std::chrono::steady_clock::time_point now)
{
    const CastingInfo* c = caster->GetCasting();
    if (!c)
        return;

    const REQ_SkillCastPacket req = c->req;
    caster->ClearCasting();
    castingPlayers_.erase(caster->GetPlayerID());

    auto failCast = [&](SkillFailReason reason) {
        SendSkillFail(caster, req.skillID, reason);
        BroadcastCastCancel(*caster, req.skillID, CastCancelReason::Failed);
    };
    const SkillData* skill = SkillTemplate::Instance().GetData(caster->GetClassType(), req.skillID);
    const std::vector<SkillEffectData>* effects =
        skill ? SkillEffectTemplate::Instance().GetGroup(skill->effectGroupID) : nullptr;
    if (!skill || !effects)
        return failCast(SkillFailReason::InvalidSkill);

    // 자원 체크
    if (!caster->CheckSkillCost(skill->costType, skill->cost))
        return failCast(SkillFailReason::NotEnoughCost);

    SkillResolve res;
    SkillFailReason reason = SkillFailReason::None;

    if (!ResolveSkillTargets(caster, *skill, req, res, reason))
        return failCast(reason);

    ExecuteSkill(caster, *skill, *effects, res, now);
}

void Zone::CancelCasting(const PlayerRef& player, CastCancelReason reason)
{
    const CastingInfo* c = player->GetCasting();
    if (!c)
        return;

    const uint32_t skillID = c->req.skillID;
    player->ClearCasting();
    castingPlayers_.erase(player->GetPlayerID());
    BroadcastCastCancel(*player, skillID, reason);
}

// 스킬 효과 적용 구간
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
            // 스킬 데미지 추출
            case SkillEffectType::Damage: {
                const int32_t raw =
                    Combat::ApplyCritical(caster->GetStat(), Combat::CalcSkillValue(caster->GetStat(), effect));
                totalDamage +=
                    monster.TakeDamage(Combat::ReduceByDefense(raw, monster.GetStat()), caster->GetPlayerID());
            }
            break;

            // 디버프 적용
            case SkillEffectType::Debuff: {
                monster.AddStatEffect(
                    effect.statType, effect.valueType, effect.value,
                    now + std::chrono::milliseconds(effect.durationMs));
            }
            break;

            // 도트 적용
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

void Zone::ApplyPlayerEffects(
    const PlayerRef& target,
    uint64_t casterID,
    const CombatStat& casterStat,
    const std::vector<SkillEffectData>& effects,
    std::chrono::steady_clock::time_point now)
{
    int32_t healHp = 0, healMp = 0;
    bool statChanged = false;

    for (const SkillEffectData& e : effects)
    {
        const auto endAt = now + std::chrono::milliseconds(e.durationMs);
        switch (e.effectType)
        {
            case SkillEffectType::Revive: {
                if (!target->IsDead())
                    break;
                const int32_t hp = std::max(1, Combat::CalcRecoverAmount(e, target->GetMaxHp()));
                target->Revive(target->GetX(), target->GetY(), target->GetZ(), hp);
                RevivePlayer(target);
                break;
            }
            case SkillEffectType::HealHp:
                healHp += Combat::CalcRecoverAmount(e, target->GetMaxHp());
                break;
            case SkillEffectType::HealMp:
                healMp += Combat::CalcRecoverAmount(e, target->GetMaxMp());
                break;
            case SkillEffectType::Buff:
            case SkillEffectType::Debuff: {
                const int32_t value = Combat::ToStatEffectValue(e);
                const EffectResult r = target->AddStatEffect(e.statType, e.valueType, value, endAt);
                if (r == EffectResult::Ignored)
                    break;
                SendEffectUpdate(target, {e.statType, e.valueType, value}, e.durationMs);
                statChanged |= (r == EffectResult::Applied);
                break;
            }
            case SkillEffectType::DotDamage:
                target->AddDot(casterID, e.dotType, Combat::CalcSkillValue(casterStat, e), now, endAt);
                break;
            default:
                break;
        }
    }

    const bool healed = target->Regen(healHp, healMp);
    if (statChanged)
        OnPlayerStatChanged(target); // 전체 스탯 + 주변 HP/MP (회복분 포함)
    else if (healed)
        BroadcastPlayerStat(target);
}

void Zone::UpdatePlayerEffects(std::chrono::steady_clock::time_point now)
{
    std::vector<StatModifier> expired;
    std::vector<PlayerRef> dead;

    for (auto& [pid, player] : players_)
    {
        if (!player->HasEffects())
            continue;

        expired.clear();

        const int32_t dotDamage = player->UpdateEffects(now, expired);
        for (const StatModifier& m : expired)
            SendEffectUpdate(player, m, 0);

        if (!expired.empty())
            OnPlayerStatChanged(player);
        else if (dotDamage > 0)
            BroadcastPlayerStat(player);

        if (dotDamage > 0 && player->IsDead())
            dead.push_back(player);
    }

    for (const PlayerRef& p : dead)
        OnPlayerDead(p);
}

void Zone::OnPlayerDead(const PlayerRef& player)
{
    CancelCasting(player, CastCancelReason::Dead);

    std::vector<StatModifier> removed;
    if (player->ClearEffects(removed))
    {
        for (const StatModifier& m : removed)
            SendEffectUpdate(player, m, 0);

        OnPlayerStatChanged(player);
    }
}

void Zone::SendEffectUpdate(const PlayerRef& player, const StatModifier& mod, int32_t remainMs)
{
    RES_BuffUpdatePacket pkt{};
    pkt.header.id = PKT_S_BUFF_UPDATE;
    pkt.header.size = sizeof(pkt);
    pkt.statType = mod.statType;
    pkt.valueType = mod.valueType;
    pkt.value = mod.value; // 음수면 디버프
    pkt.remainMs = remainMs;
    SendTo(player, &pkt, sizeof(pkt));
}