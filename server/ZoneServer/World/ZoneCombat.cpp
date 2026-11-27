#include "pch.h"
#include "World/Zone.h"
#include "Object/Player.h"
#include "Object/Monster.h"

#include "Combat/Combat.h"
#include "Combat/HitArea.h"

#include "World/ZoneConstants.h"

#include "Util/MathUtil.h"

#include "Template/ZoneTemplate.h"
#include "Template/SkillTemplate.h"
#include "Template/SkillEffectTemplate.h"

#include "ServerCore/Manager/SendBufferManager.h"
#include "ServerCore/Protocol/Packets.h"

// Zone 전투 멤버 함수 (기본 공격, 스킬, 사망/부활)
// 전투 공식은 Combat.h, 상태 관리는 Player / Monster 쪽

// 조건이 하나라도 만족하면 true
static bool HasEffect(const std::vector<SkillEffectData>& effects, SkillEffectType type)
{
    return std::any_of(
        effects.begin(), effects.end(), [type](const SkillEffectData& e) { return e.effectType == type; });
}

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

void Zone::CollectAlliesInArea(
    const HitArea& area, uint64_t casterID, size_t maxCount, bool deadOnly, std::vector<PlayerRef>& out) const
{
    std::vector<std::pair<float, PlayerRef>> found;

    playerGrid_.ForEachInRange(area.ox, area.oz, area.BoundRadius(), [&](uint64_t playerID) {
        // 본인 + 같은 파티원
        if (!IsSameParty(casterID, playerID))
            return;

        auto it = players_.find(playerID);
        if (it == players_.end())
            return;

        const PlayerRef& p = it->second;
        if (p->IsDead() != deadOnly)
            return;
        if (portalPending_.count(playerID))
            return;
        if (!area.Contains(p->GetX(), p->GetZ()))
            return;

        // 범위 내 파티원 정보 저장
        found.emplace_back(Math::DistSqXZ(area.ox, area.oz, p->GetX(), p->GetZ()), p);
    });

    // 가장 가까운 파티원 n 만큼 정렬
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
    if (skill->targetType == TargetType::None)
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
    if (!ResolveSkillTargets(caster, *skill, *effects, req, res, reason))
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
    pkt.dirX = res.dirX;
    pkt.dirZ = res.dirZ;
    pkt.aimX = res.aimX;
    pkt.aimY = res.aimY;
    pkt.aimZ = res.aimZ;

    if (res.mainTarget)
    {
        pkt.targetType = ObjectType::Monster;
        pkt.targetUID = res.mainTarget->GetMonsterUID();
    }
    else if (res.mainPlayer)
    {
        pkt.targetType = ObjectType::Player;
        pkt.targetUID = res.mainPlayer->GetPlayerID();
    }

    BroadcastNear(CellOf(*caster), &pkt, sizeof(pkt));
}

bool Zone::ResolveSkillTargets(
    const PlayerRef& caster,
    const SkillData& skill,
    const std::vector<SkillEffectData>& effects,
    const REQ_SkillCastPacket& req,
    SkillResolve& out,
    SkillFailReason& reason) const
{
    const uint64_t casterID = caster->GetPlayerID();
    const float cx = caster->GetX(), cy = caster->GetY(), cz = caster->GetZ();

    float dirX = req.dirX, dirZ = req.dirZ;
    const bool hasDir = Math::NormalizeXZ(dirX, dirZ);
    out.aimX = cx;
    out.aimY = cy;
    out.aimZ = cz;

    // 지정 대상 1마리 (Single / TargetArea 공용): 타입 → 존재/상태 → 사거리
    auto fail = [&](SkillFailReason r) {
        reason = r;
        return false;
    };

    // 1. 본인 대상
    if (skill.targetType == TargetType::Self)
    {
        out.players.push_back(caster);
        out.dirX = hasDir ? dirX : 0.f;
        out.dirZ = hasDir ? dirZ : 0.f;
        return true;
    }

    const bool ally = skill.targetType == TargetType::Ally;
    const bool revive = HasEffect(effects, SkillEffectType::Revive);
    const float maxRange = skill.range + ZoneConst::ATTACK_RANGE_TOLERANCE;

    // 2. 지정 대상 스킬 (single/ TargetArea)
    float ox = cx, oz = cz; // 원형 판정 중심 (기본: 시전자)
    if (skill.rangeType == RangeType::Single || skill.rangeType == RangeType::TargetArea)
    {
        // 파티 대상
        if (ally)
        {
            // 대상 지정이 없으면 본인
            PlayerRef p = caster;
            if (req.targetType == ObjectType::Player)
            {
                auto it = players_.find(req.targetUID);
                p = it != players_.end() ? it->second : nullptr;
            }

            if (!p || portalPending_.count(p->GetPlayerID()))
                return fail(SkillFailReason::InvalidTarget);
            if (!IsSameParty(casterID, p->GetPlayerID()))
                return fail(SkillFailReason::InvalidTarget);
            if (p->IsDead() != revive)
                return fail(SkillFailReason::InvalidTarget);
            if (!Math::IsWithinRange3D(cx, cy, cz, p->GetX(), p->GetY(), p->GetZ(), maxRange))
                return fail(SkillFailReason::OutOfRange);
            out.mainPlayer = p;
            out.aimX = p->GetX();
            out.aimY = p->GetY();
            out.aimZ = p->GetZ();
        }
        else
        {
            // 몬스터 대상
            if (req.targetType != ObjectType::Monster)
                return fail(SkillFailReason::InvalidTarget);
            auto it = monsters_.find(req.targetUID);
            if (it == monsters_.end() || it->second->IsDead() || it->second->GetState() == MonsterState::Return)
                return fail(SkillFailReason::InvalidTarget);

            Monster* m = it->second.get();
            if (m->DistSqTo(cx, cy, cz) > maxRange * maxRange)
                return fail(SkillFailReason::OutOfRange);

            out.mainTarget = m;
            out.aimX = m->GetX();
            out.aimY = m->GetY();
            out.aimZ = m->GetZ();
        }
        ox = out.aimX;
        oz = out.aimZ;
    }

    // 판정 범위
    HitArea area;
    switch (skill.rangeType)
    {
        case RangeType::Single: {
            break;
        }
        case RangeType::SelfArea:
        case RangeType::TargetArea: {
            area = HitArea::Circle(ox, oz, skill.radius);
            break;
        }
        case RangeType::Cone: {
            if (!hasDir)
                return fail(SkillFailReason::InvalidAim);
            area = HitArea::Cone(cx, cz, dirX, dirZ, skill.range, skill.angle);
            break;
        }
        case RangeType::Line: {
            if (!hasDir)
                return fail(SkillFailReason::InvalidAim);
            area = HitArea::Line(cx, cz, dirX, dirZ, skill.range, skill.width);
            break;
        }
        case RangeType::GroundArea: {
            if (!Math::IsFinite3(req.aimX, req.aimY, req.aimZ))
                return fail(SkillFailReason::InvalidAim);
            if (!Math::IsWithinRangeXZ(cx, cz, req.aimX, req.aimZ, maxRange))
                return fail(SkillFailReason::OutOfRange);

            out.aimX = req.aimX;
            out.aimY = req.aimY;
            out.aimZ = req.aimZ;
            area = HitArea::Circle(req.aimX, req.aimZ, skill.radius);
            break;
        }
        default:
            return fail(SkillFailReason::InvalidSkill);
    }

    // 4. 대상 수집
    if (skill.rangeType == RangeType::Single)
    {
        if (out.mainTarget)
            out.targets.push_back(out.mainTarget);
        if (out.mainPlayer)
            out.players.push_back(out.mainPlayer);
    }
    else if (ally)
        CollectAlliesInArea(area, casterID, skill.targetCount, revive, out.players);
    else
        CollectMonstersInArea(area, skill.targetCount, out.targets);

    // ===== 방향: 지정 대상이 있으면 대상 쪽 =====
    if (out.mainTarget || out.mainPlayer)
    {
        dirX = out.aimX - cx;
        dirZ = out.aimZ - cz;
        if (!Math::NormalizeXZ(dirX, dirZ)) // 본인에게 쓴 경우 등
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
    hits.reserve(res.targets.size() + res.players.size());

    // 플레이어 대상(본인 / 아군): 회복, 버프, 부활
    for (const PlayerRef& p : res.players)
    {
        ApplyPlayerEffects(p, caster->GetPlayerID(), caster->GetStat(), effects, now);

        SkillHit hit{};
        hit.targetType = ObjectType::Player;
        hit.targetUID = p->GetPlayerID();
        hit.targetHp = p->GetHp();
        hits.push_back(hit);
    }

    // 몬스터 대상 : 데미지, 디버프, 도트
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
        AoiCell center = res.mainTarget   ? CellOf(*res.mainTarget)
                         : res.mainPlayer ? CellOf(*res.mainPlayer)
                                          : CellOf(*caster);
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

    if (!ResolveSkillTargets(caster, *skill, *effects, req, res, reason))
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