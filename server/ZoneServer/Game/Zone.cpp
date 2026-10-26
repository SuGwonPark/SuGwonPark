#include "pch.h"
#include "Zone.h"
#include "Player.h"

#include "Network/GatewaySession.h"

#include "Game/ZoneConstants.h"
#include "Game/Monster.h"

#include "Template/PortalTemplate.h"
#include "Template/MonsterSpawnTemplate.h"
#include "Template/MonsterTemplate.h"
#include "Template/RewardTemplate.h"
#include "Template/ZoneTemplate.h"
#include "Template/SkillTemplate.h"

#include "ServerCore/Manager/SendBufferManager.h"
#include "ServerCore/Utils/util.h"
#include "ServerCore/DB/GameDB.h"
#include "ServerCore/Protocol/InternalPackets.h"

static PlayerInfo ToInfo(const Player& p)
{
    PlayerInfo info{};
    info.playerID = p.GetPlayerID();
    strncpy_s(info.name, p.GetName().c_str(), _TRUNCATE);
    info.x = p.GetX();
    info.y = p.GetY();
    info.z = p.GetZ();
    info.hp = p.GetHp();
    return info;
}

static MonsterInfo ToInfo(const Monster& m)
{
    MonsterInfo info{};
    info.monsterUID = m.GetMonsterUID();
    info.monsterID = m.GetMonsterID();
    info.hp = m.GetHp();
    info.maxHp = m.GetMaxHp();
    info.x = m.GetX();
    info.y = m.GetY();
    info.z = m.GetZ();
    return info;
}

static RES_PlayerStatPacket MakeStatPacket(const Player& player)
{
    RES_PlayerStatPacket pkt{};
    pkt.header.id = PKT_S_PLAYER_STAT;
    pkt.header.size = sizeof(pkt);
    pkt.playerID = player.GetPlayerID();
    pkt.hp = player.GetHp();
    pkt.maxHp = player.GetMaxHp();
    pkt.mp = player.GetMp();
    pkt.maxMp = player.GetMaxMp();
    return pkt;
}

Zone::Zone(uint32_t zoneID, uint32_t channelID) : zoneID_(zoneID), channelID_(channelID)
{
}

void Zone::InitMonster()
{
    for (const auto& sp : MonsterSpawnTemplate::Instance().GetByZone(zoneID_))
    {
        const MonsterData* data = MonsterTemplate::Instance().GetData(sp.monsterID);
        const uint64_t monsterUID = ++nextMonsterUID_;
        monsters_[monsterUID] = std::make_shared<Monster>(monsterUID, data, sp.x, sp.y, sp.z);
    }
}

void Zone::Enter(PlayerRef player)
{
    // 1) 나에게: 입장 결과
    RES_EnterGamePacket enter{};
    enter.header.id = PKT_S_ENTER_GAME;
    enter.header.size = sizeof(enter);
    enter.zoneID = zoneID_;
    enter.channelID = channelID_;
    enter.me = ToInfo(*player);
    enter.mp = player->GetMp();
    enter.exp = player->GetExp();
    SendTo(player, &enter, sizeof(enter));

    // 2) 나에게: 이미 있던 플레이어 목록 (아직 등록 전이라 나 자신은 안 들어간다)
    if (!players_.empty())
    {
        std::vector<PlayerInfo> others;
        others.reserve(players_.size());
        for (auto& [id, p] : players_)
            others.push_back(ToInfo(*p));

        RES_SpawnPacket header{};
        header.header.id = PKT_S_SPAWN;
        header.count = static_cast<uint16_t>(others.size());

        // GatewayServer -> 클라이언트
        if (SendBufferRef buf = SendBufferManager::MakeList(header, others))
            SendTo(player, buf->Buffer(), static_cast<uint16_t>(buf->AllocSize()));
    }

    // 나에게 몬스터 목록을 알려준다
    SendMonsterList(player);

    // 3) 기존 플레이어들에게: 새 플레이어 등장
    {
        std::vector<PlayerInfo> me{ToInfo(*player)};
        RES_SpawnPacket header{};
        header.header.id = PKT_S_SPAWN;
        header.count = 1;
        if (SendBufferRef buf = SendBufferManager::MakeList(header, me))
            BroadCast(buf->Buffer(), static_cast<uint16_t>(buf->AllocSize()));
    }

    // 4) 마지막에 내 정보 등록
    players_[player->GetPlayerID()] = player;
    sessionToPlayer_[player->GetSessionID()] = player;
    // 주변 시야(AOI) 내 유저들에게 입장 알림 브로드캐스트
}

void Zone::Leave(uint64_t playerID)
{
    auto it = players_.find(playerID);
    if (it == players_.end())
        return;

    sessionToPlayer_.erase(it->second->GetSessionID());
    players_.erase(it);

    // 이미 지웟으므로 남은 사람에게만 보냄
    RES_DespawnPacket pkt{};
    pkt.header.id = PKT_S_DESPAWN;
    pkt.header.size = sizeof(pkt);
    pkt.playerID = playerID;
    BroadCast(&pkt, sizeof(pkt));
}

void Zone::HandleDisconnect(uint64_t sessionID)
{
    auto self = shared_from_this();
    // 스케쥴 시작
    Post([self, sessionID]() {
        self->spawnPending_.erase(sessionID);

        // 세션 ID 확인 없으면 종료
        auto it = self->sessionToPlayer_.find(sessionID);
        if (it == self->sessionToPlayer_.end())
            return;

        // 세션 캐릭터 정보
        PlayerRef player = it->second;
        const uint64_t playerID = player->GetPlayerID();

        // 포탈 이동중이 아닐 경우
        if (self->portalPending_.count(playerID) == 0)
        {
            // 캐릭터 정보 저장
            GameDB::Instance().SaveCharacterProgressAsync(playerID,
                                                          player->GetExp(),
                                                          player->GetX(),
                                                          player->GetY(),
                                                          player->GetZ(),
                                                          player->GetHp(),
                                                          player->GetMp(),
                                                          self->GetZoneID());
        }
        // 퇴장 처리
        self->Leave(player->GetPlayerID());
    });
}

void Zone::HandleClientPacket(uint64_t sessionID, uint8_t* payload, uint16_t size)
{
    if (size < sizeof(PacketHeader))
        return;

    std::vector<uint8_t> copied(payload, payload + size);

    auto self = shared_from_this();
    Post([self, sessionID, copied = std::move(copied)]() mutable {
        PacketHeader* header = reinterpret_cast<PacketHeader*>(copied.data());

        switch (header->id)
        {
                // 스폰 준비
            case PKT_C_READY_TO_SPAWN: {
                self->HandleReadyToSpawn(sessionID, copied.data(), static_cast<uint16_t>(copied.size()));
                break;
            }
                // 캐릭터 이동
            case PKT_C_MOVE: {
                if (copied.size() < sizeof(MovePacket))
                    return;

                auto it = self->sessionToPlayer_.find(sessionID);
                // 아직 Enter 안 한 세션이 이동 패킷 보냄 -> 무시
                if (it == self->sessionToPlayer_.end())
                    return;

                auto* move = reinterpret_cast<MovePacket*>(copied.data());

                self->HandleMove(it->second->GetPlayerID(), move->x, move->y, move->z);
                break;
            }
                // 포탈 이동
            case PKT_C_PORTAL: {
                if (copied.size() < sizeof(REQ_PortalPacket))
                    return;

                auto it = self->sessionToPlayer_.find(sessionID);
                if (it == self->sessionToPlayer_.end())
                    return;

                auto* req = reinterpret_cast<REQ_PortalPacket*>(copied.data());
                self->HandlePortalRequest(it->second, req->portalID);
                break;
            }

            case PKT_C_ATTACK: {
                if (copied.size() < sizeof(REQ_AttackPacket))
                    return;

                auto it = self->sessionToPlayer_.find(sessionID);
                if (it == self->sessionToPlayer_.end())
                    return;

                auto* req = reinterpret_cast<REQ_AttackPacket*>(copied.data());
                self->HandleAttack(it->second, req->targetType, req->targetUID);
                break;
            }

            case PKT_C_REVIVE: {
                auto it = self->sessionToPlayer_.find(sessionID);
                if (it == self->sessionToPlayer_.end())
                    return;
                self->HandleRevive(it->second);
                break;
            }

            case PKT_C_SKILL_CAST: {
                if (copied.size() < sizeof(REQ_SkillCastPacket))
                    return;

                auto it = self->sessionToPlayer_.find(sessionID);
                if (it == self->sessionToPlayer_.end())
                    return;

                auto* req = reinterpret_cast<REQ_SkillCastPacket*>(copied.data());
                self->HandleSkillCast(it->second, req->skillID, req->targetType, req->targetUID);
                break;
            }
            default:
                break;
        }
    });
}

void Zone::SaveAllAndClear(std::function<void()> onDone)
{
    auto self = shared_from_this();
    Post([self, onDone = std::move(onDone)]() {
        for (auto& [playerID, player] : self->players_)
        {
            if (self->portalPending_.count(playerID))
                continue; // 포탈 저장이 최종 상태
            GameDB::Instance().SaveCharacterProgressAsync(playerID,
                                                          player->GetExp(),
                                                          player->GetX(),
                                                          player->GetY(),
                                                          player->GetZ(),
                                                          player->GetHp(),
                                                          player->GetMp(),
                                                          self->zoneID_);
        }
        self->players_.clear();
        self->sessionToPlayer_.clear();
        self->spawnPending_.clear();

        if (onDone)
            onDone(); // 저장 "요청"까지 끝났다는 신호
    });
}

void Zone::HandleReadyToSpawn(uint64_t sessionID, uint8_t* payload, uint16_t size)
{
    if (size < sizeof(REQ_ReadyToSpawnPacket))
        return;
    uint64_t playerID = reinterpret_cast<REQ_ReadyToSpawnPacket*>(payload)->playerID;

    // 중복 스폰 방지
    if (sessionToPlayer_.count(sessionID))
        return;
    // 이미 진행중이면 무시
    if (!spawnPending_.insert(sessionID).second)
        return;

    auto self = shared_from_this();

    GameDB::Instance().PushTask(playerID, [self, sessionID, playerID](mysqlx::Session& db) {
        PlayerRef player;
        std::exception_ptr error;
        try
        {
            auto row = db.sql("SELECT c.name, cs.PosX, cs.PosY, cs.PosZ, cs.hp, cs.mp, cs.exp "
                              "FROM `character` c JOIN `character_state` cs "
                              "ON c.playerID = cs.playerID "
                              "WHERE c.playerID = ? AND c.deleteTime IS NULL")
                           .bind(playerID)
                           .execute()
                           .fetchOne();
            if (row)
            {

                std::string name = row[0].get<std::string>();
                float x = row[1].get<float>(), y = row[2].get<float>(), z = row[3].get<float>();
                int hp = row[4].get<int>();
                int mp = row[5].get<int>();
                uint64_t exp = row[6].get<uint64_t>();

                player = std::make_shared<Player>(playerID, name, x, y, z, hp, mp, exp);
                player->SetSessionID(sessionID);

                // 죽은 플레이어 재접속시 살릴지 말지
                // if (player->IsDead()) {
                //	if (const ZoneData* zone = ZoneTemplate::Instance().GetData(self->GetZoneID()))
                // player->Revive(zone->spawnX, zone->spawnY, zone->spawnZ, player->GetMaxHp());
                //}
            }
        }
        catch (const mysqlx::Error& e)
        {
            std::cerr << "[Spawn] DB 로드 실패:" << e.what() << std::endl;
            error = std::current_exception();
        }

        self->Post([=]() {
            // true = HandleDisconnect에서 지워진거임
            if (self->spawnPending_.erase(sessionID) == 0)
                return;
            if (!player)
                return;

            // zone에 캐릭터가 이미 있으면 종료
            if (self->players_.count(playerID))
            {
                std::cerr << "[Spawn] 이미 Zone에 있는 캐릭터: " << playerID << std::endl;
                return;
            }

            self->Enter(player);
        });

        if (error)
            std::rethrow_exception(error);
    });
}

void Zone::SendMonsterList(const PlayerRef& player)
{
    if (monsters_.empty())
        return;

    std::vector<MonsterInfo> list;
    list.reserve(monsters_.size());
    for (auto& [uid, m] : monsters_)
    {
        if (m->IsDead())
            continue; // 사망한 몬스터
        list.push_back(ToInfo(*m));
    }
    if (list.empty())
        return;

    RES_MonsterSpawnPacket pkt{};
    pkt.header.id = PKT_S_MONSTER_SPAWN;
    pkt.count = static_cast<uint16_t>(list.size());

    if (SendBufferRef buf = SendBufferManager::MakeList(pkt, list))
    {
        SendTo(player, buf->Buffer(), static_cast<uint16_t>(buf->AllocSize()));
    }
}

void Zone::RequestTick()
{
    if (tickPending_.exchange(true))
        return;

    auto self = shared_from_this();
    Post([self]() {
        self->tickPending_.store(false);
        self->Update();
    });
}

void Zone::Update()
{
    // 현재 시간
    auto now = std::chrono::steady_clock::now();
    // 경과 시간 (float 형식 = 초 단위)
    float dt = std::chrono::duration<float>(now - lastTick_).count();
    lastTick_ = now;
    dt = std::min(dt, ZoneConst::MAX_TICK_DT);

    for (auto& [uid, monster] : monsters_)
    {
        // 죽어 있으면 리스폰 시간 체크하여 리스폰 시킴
        if (monster->IsDead())
        {
            if (monster->CanRespawn(now))
                RespawnMonster(*monster);
            continue;
        }
        UpdateMonster(*monster, dt, now);
    }

    // 플레이어 자동 회복 (1초마다 진행)
    regenTimer_ += dt;
    if (regenTimer_ >= ZoneConst::PLAYER_REGEN_INTERVAL_SEC)
    {
        regenTimer_ -= ZoneConst::PLAYER_REGEN_INTERVAL_SEC;
        RegenPlayers();
    }
}

void Zone::UpdateMonster(Monster& monster, float dt, std::chrono::steady_clock::time_point now)
{
    if (monster.IsDead())
        return;

    const MonsterData& data = monster.getData();
    const float step = data.moveSpeed * dt; // 틱에 따른 갈 수 있는 거리
    bool moved = false;

    // 몬스터 상태에 따른 행동
    switch (monster.GetState())
    {
        case MonsterState::Idle: {
            // 플레이어 타깃 체크
            if (PlayerRef target = FindNearestPlayer(monster, data.aggroRange))
                monster.StartChase(target->GetPlayerID());
            break;
        }
        case MonsterState::Chase: {

            // 복귀 거리
            const float leash = ZoneConst::MONSTER_LEASH_RANGE;

            // 일정 거리 이상 가면 초기화
            if (monster.DistSqFromSpawn() > leash * leash)
            {
                monster.StartReturn();
                break;
            }

            PlayerRef target;
            auto it = players_.find(monster.GetTargetID());
            // 타겟이 없거나 죽으면 새로운 타깃 찾기
            if (it != players_.end() && !it->second->IsDead())
                target = it->second;
            else
                target = FindNearestPlayer(monster, data.aggroRange);

            if (!target)
            {
                monster.StartReturn();
                break;
            }

            // 공격 범위까지 접근
            moved = monster.MoveToward(target->GetX(), target->GetY(), target->GetZ(), step, data.attackRange * 0.9f);

            // 공격 범위 안이면 쿨타임 체크하여 공격
            const float atk = data.attackRange;
            if (monster.DistSqTo(target->GetX(), target->GetY(), target->GetZ()) <= atk * atk &&
                monster.TryUseAttack(now, std::chrono::milliseconds(ZoneConst::MONSTER_ATTACK_COOLDOWN_MS)))
            {
                MonsterAttack(monster, target);
            }
            break;
        }
        case MonsterState::Return: {
            // 스폰 위치 확인
            moved = monster.MoveToward(monster.GetSpawnX(), monster.GetSpawnY(), monster.GetSpawnZ(), step, 0.f);

            const float eps = ZoneConst::MONSTER_ARRIVE_EPSILON;
            if (monster.DistSqFromSpawn() <= eps * eps)
            {
                monster.SetIdle();
                if (monster.ResetHp())
                    BroadcastMonsterHp(monster);
            }
            break;
        }
    }
    if (moved)
        BroadcastMonsterMove(monster);
}

PlayerRef Zone::FindNearestPlayer(const Monster& monster, float range) const
{
    PlayerRef nearest;

    float bestSq = range * range;

    for (const auto& [playerID, player] : players_)
    {
        if (portalPending_.count(playerID))
            continue;
        if (player->IsDead())
            continue;

        // 플레이어 위치 체크
        float dsq = monster.DistSqTo(player->GetX(), player->GetY(), player->GetZ());
        // 이전 플레이어보다 가까우면 교체
        if (dsq <= bestSq)
        {
            bestSq = dsq;
            nearest = player;
        }
    }

    return nearest;
}

void Zone::BroadcastMonsterMove(const Monster& monster)
{
    RES_MonsterMovePacket pkt{};
    pkt.header.id = PKT_S_MONSTER_MOVE;
    pkt.header.size = sizeof(pkt);
    pkt.monsterUID = monster.GetMonsterUID();
    pkt.x = monster.GetX();
    pkt.y = monster.GetY();
    pkt.z = monster.GetZ();
    BroadCast(&pkt, sizeof(pkt));
}

void Zone::HandleAttack(const PlayerRef& attacker, ObjectType targetType, uint64_t targetUID)
{
    if (attacker->IsDead())
        return;

    const uint64_t attackerUID = attacker->GetPlayerID();
    if (portalPending_.count(attackerUID))
        return;
    if (targetType != ObjectType::Monster)
        return;

    auto it = monsters_.find(targetUID);
    if (it == monsters_.end())
        return;
    Monster& monster = *it->second;

    // 죽었거나 복귀중이면 무시
    if (monster.IsDead())
        return;
    if (monster.GetState() == MonsterState::Return)
        return;

    // 사거리 체크
    const float range = ZoneConst::PLAYER_ATTACK_RANGE + ZoneConst::ATTACK_RANGE_TOLERANCE;
    if (monster.DistSqTo(attacker->GetX(), attacker->GetY(), attacker->GetZ()) > range * range)
        return;

    // 공격 쿨타임
    const auto now = std::chrono::steady_clock::now();
    if (!attacker->TryUseAttack(now, std::chrono::milliseconds(ZoneConst::PLAYER_ATTACK_COOLDOWN_MS)))
        return;

    // 데미지
    const int32_t damage = monster.TakeDamage(ZoneConst::PLAYER_ATTACK_DAMAGE);

    if (monster.GetState() == MonsterState::Idle)
    {
        monster.StartChase(attackerUID);
    }

    RES_AttackPacket pkt{};
    pkt.header.id = PKT_S_ATTACK;
    pkt.header.size = sizeof(pkt);
    pkt.attackerType = ObjectType::Player;
    pkt.attackerUID = attackerUID;
    pkt.targetType = ObjectType::Monster;
    pkt.targetUID = monster.GetMonsterUID();
    pkt.damage = damage;
    pkt.targetHp = monster.GetHp();
    BroadCast(&pkt, sizeof(pkt));

    if (monster.GetHp() <= 0)
    {
        OnMonsterDead(monster, attacker);
    }
}

void Zone::OnMonsterDead(Monster& monster, const PlayerRef& killer)
{
    monster.Die(std::chrono::steady_clock::now());

    RES_MonsterDespawnPacket pkt{};
    pkt.header.id = PKT_S_MONSTER_DESPAWN;
    pkt.header.size = sizeof(pkt);
    pkt.monsterUID = monster.GetMonsterUID();
    BroadCast(&pkt, sizeof(pkt));

    std::vector<RewardResult> rewards;
    RewardTemplate::Instance().Roll(monster.getData().rewardGroupID, rewards);
    GiveRewards(killer, rewards);
}

void Zone::RespawnMonster(Monster& monster)
{
    monster.Respawn();

    RES_MonsterSpawnPacket pkt{};
    pkt.header.id = PKT_S_MONSTER_SPAWN;
    pkt.header.size = sizeof(pkt);
    pkt.count = 1;

    std::vector<MonsterInfo> one{ToInfo(monster)};
    if (SendBufferRef buf = SendBufferManager::MakeList(pkt, one))
        BroadCast(buf->Buffer(), static_cast<uint16_t>(buf->AllocSize()));
}

void Zone::BroadcastMonsterHp(const Monster& monster)
{
    RES_MonsterHpPacket pkt{};
    pkt.header.id = PKT_S_MONSTER_HP;
    pkt.header.size = sizeof(pkt);
    pkt.monsterUID = monster.GetMonsterUID();
    pkt.hp = monster.GetHp();
    BroadCast(&pkt, sizeof(pkt));
}

void Zone::MonsterAttack(Monster& monster, const PlayerRef& target)
{
    const int32_t damage = target->TakeDamage(monster.getData().attack);

    RES_AttackPacket pkt{};
    pkt.header.id = PKT_S_ATTACK;
    pkt.header.size = sizeof(pkt);
    pkt.attackerType = ObjectType::Monster;
    pkt.attackerUID = monster.GetMonsterUID();
    pkt.targetType = ObjectType::Player;
    pkt.targetUID = target->GetPlayerID();
    pkt.damage = damage;
    pkt.targetHp = target->GetHp();
    BroadCast(&pkt, sizeof(pkt));
}

void Zone::HandleRevive(const PlayerRef& player)
{
    if (!player->IsDead())
        return;

    const ZoneData* zone = ZoneTemplate::Instance().GetData(zoneID_);
    if (!zone)
        return;

    player->Revive(zone->spawnX, zone->spawnY, zone->spawnZ, player->GetMaxHp());

    RES_RevivePacket pkt{};
    pkt.header.id = PKT_S_REVIVE;
    pkt.header.size = sizeof(pkt);
    pkt.playerID = player->GetPlayerID();
    pkt.x = player->GetX();
    pkt.y = player->GetY();
    pkt.z = player->GetZ();
    pkt.hp = player->GetHp();
    BroadCast(&pkt, sizeof(pkt));
}

void Zone::GiveRewards(const PlayerRef& player, const std::vector<RewardResult>& rewards)
{
    uint64_t gainedExp = 0;
    for (const auto& r : rewards)
    {
        if (r.delivery == RewardDeliveryType::Direct && r.type == RewardItemType::Exp)
        {
            gainedExp += r.count;
            continue;
        }

        // 나머지 보상도 추가 해야함
    }

    if (gainedExp > 0)
    {
        player->AddExp(gainedExp);

        RES_ExpPacket pkt{};
        pkt.header.id = PKT_S_EXP;
        pkt.header.size = sizeof(pkt);
        pkt.gainedExp = gainedExp;
        pkt.totalExp = player->GetExp();
        SendTo(player, &pkt, sizeof(pkt));
    }
}

void Zone::HandleSkillCast(const PlayerRef& caster, uint32_t skillID, ObjectType targetType, uint64_t targetUID)
{
    const uint64_t casterID = caster->GetPlayerID();
    if (portalPending_.count(casterID))
        return;

    auto fail = [&](SkillFailReason reason) { SendSkillFail(caster, skillID, reason); };

    if (caster->IsDead())
        return fail(SkillFailReason::Dead);

    const SkillData* skill = SkillTemplate::Instance().GetData(skillID);
    if (!skill)
        return fail(SkillFailReason::InvalidSkill);

    const auto now = std::chrono::steady_clock::now();
    // 스킬 쿨타임 확이
    if (!caster->IsSkillReady(skillID, now))
        return fail(SkillFailReason::Cooldown);
    // 스킬 코스트 확인
    if (!caster->CheckSKillCost(skill->costType, skill->cost))
        return fail(SkillFailReason::NotEnoughCost);

    // 지정 대상 1마리 (Single / TargetArea 공용): 타입 → 존재/상태 → 사거리 순으로 검사
    auto findTarget = [&](SkillFailReason& reason) -> Monster* {
        if (targetType != ObjectType::Monster)
        {
            reason = SkillFailReason::InvalidTarget;
            return nullptr;
        }

        auto it = monsters_.find(targetUID);
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

    std::vector<Monster*> targets;
    SkillFailReason reason = SkillFailReason::None;
    Monster* target{};
    switch (skill->targetType)
    {
        case SkillTargetType::Single: {
            target = findTarget(reason);
            if (!target)
                return fail(reason);
            targets.push_back(target);
            break;
        }
        case SkillTargetType::SelfArea: {
            CollectMonstersInRadius(
                caster->GetX(), caster->GetY(), caster->GetZ(), skill->radius, ZoneConst::SKILL_MAX_TARGETS, targets);
            break;
        }
        case SkillTargetType::TargetArea: {
            target = findTarget(reason);
            if (!target)
                return fail(reason);

            CollectMonstersInRadius(
                target->GetX(), target->GetY(), target->GetZ(), skill->radius, ZoneConst::SKILL_MAX_TARGETS, targets);
            break;
        }
        case SkillTargetType::None: {
            return fail(SkillFailReason::InvalidSkill);
        }
    }

    // 스킬 확정 이후 처리: 자원 처리 -> 데미지 -> 결과 -> 사망처리
    caster->TrySpendSkill(skill->costType, skill->cost);
    caster->StartSkillCooldown(skillID, now, std::chrono::milliseconds(skill->cooldownMs));

    std::vector<SkillHit> hits;
    hits.reserve(targets.size());
    for (Monster* m : targets)
    {
        const int32_t damage = m->TakeDamage(skill->damage);
        if (m->GetState() == MonsterState::Idle)
            m->StartChase(casterID);

        SkillHit hit{};
        hit.targetType = ObjectType::Monster;
        hit.targetUID = m->GetMonsterUID();
        hit.damage = damage;
        hit.targetHp = m->GetHp();

        hits.push_back(hit);
    }
    RES_SkillCastPacket pkt{};
    pkt.header.id = PKT_S_SKILL_CAST;
    pkt.casterID = casterID;
    pkt.skillID = skillID;
    pkt.casterHp = caster->GetHp();
    pkt.casterMp = caster->GetMp();
    pkt.hitCount = static_cast<uint16_t>(hits.size());
    if (SendBufferRef buf = SendBufferManager::MakeList(pkt, hits))
        BroadCast(buf->Buffer(), static_cast<uint16_t>(buf->AllocSize()));

    for (Monster* m : targets)
    {
        if (m->GetHp() <= 0)
            OnMonsterDead(*m, caster);
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

void Zone::CollectMonstersInRadius(
    float cx, float cy, float cz, float radius, size_t maxCount, std::vector<Monster*>& out) const
{
    std::vector<std::pair<float, Monster*>> found; // <거리 제곱, 몬스터>
    const float rSq = radius * radius;

    for (auto& [uid, m] : monsters_)
    {
        if (m->IsDead() || m->GetState() == MonsterState::Return)
            continue;
        const float dSq = m->DistSqTo(cx, cy, cz);
        if (dSq <= rSq)
            found.emplace_back(dSq, m.get());
    }

    // 앞쪽 n개만 정렬
    const size_t n = std::min(found.size(), maxCount);
    std::partial_sort(
        found.begin(), found.begin() + n, found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    for (size_t i = 0; i < n; ++i)
        out.push_back(found[i].second);
}

void Zone::RegenPlayers()
{
    for (auto& [pid, player] : players_)
    {
        if (portalPending_.count(pid))
            continue;
        if (player->Regen(ZoneConst::PLAYER_HP_REGEN, ZoneConst::PLAYER_MP_REGEN))
            BroadcastPlayerStat(player);
    }
}

void Zone::BroadcastPlayerStat(const PlayerRef& player)
{
    RES_PlayerStatPacket pkt = MakeStatPacket(*player);
    BroadCast(&pkt, sizeof(pkt));
}

void Zone::SendPlayerStat(const PlayerRef& to, const PlayerRef& player)
{
    RES_PlayerStatPacket pkt = MakeStatPacket(*player);
    SendTo(to, &pkt, sizeof(pkt));
}

void Zone::HandleMove(uint64_t playerID, float x, float y, float z)
{
    if (portalPending_.count(playerID))
        return; // 포탈 저장 중에는 위치를 고정

    auto it = players_.find(playerID);
    if (it == players_.end())
        return;
    PlayerRef& player = it->second;
    if (player->IsDead())
        return;

    MovePacket pkt{};
    pkt.header.size = sizeof(MovePacket);
    pkt.header.id = PKT_S_MOVE;
    pkt.playerID = playerID;

    if (!player->TryMove(x, y, z, ZoneConst::MAX_MOVE_SPEED))
    {
        // 비정상적인 움직임 감지 현재 포지션값을 넘긴다
        pkt.x = player->GetX();
        pkt.y = player->GetY();
        pkt.z = player->GetZ();
        SendTo(player, &pkt, sizeof(pkt));
        return;
    }

    pkt.x = x;
    pkt.y = y;
    pkt.z = z;

    // 존의 모든 유저에 전달
    BroadCast(&pkt, sizeof(pkt), playerID);
}

void Zone::HandlePortal(uint64_t playerID, uint32_t portalID)
{
    auto it = players_.find(playerID);
    if (it == players_.end())
        return;

    const PortalData* portalData = PortalTemplate::Instance().GetData(portalID);
    if (!portalData)
        return;

    // 이미 포탈 처리 중이면 무시
    if (!portalPending_.insert(playerID).second)
        return;

    PlayerRef player = it->second;
    const uint64_t sessionID = player->GetSessionID();

    auto self = shared_from_this();

    GameDB::Instance().SaveCharacterProgressAsync(
        playerID,
        player->GetExp(),
        portalData->destX,
        portalData->destY,
        portalData->destZ,
        player->GetHp(),
        player->GetMp(),
        portalData->destZoneID,
        // 콜백 DB 스레드에서 실행
        [self, sessionID, playerID, portalData](bool ok) {
            self->Post([=]() {
                // 포탈 로딩 종료
                self->portalPending_.erase(playerID);
                if (!ok)
                    return;

                auto it = self->players_.find(playerID);
                if (it == self->players_.end() || it->second->GetSessionID() != sessionID)
                    return;

                // 1. Zone 월드에서 서버 제거 (추가 피격/상호작용 방지)
                self->Leave(playerID);

                RES_ZoneLeaveCompletedPacket ackPkt{};
                ackPkt.header.size = sizeof(ackPkt);
                ackPkt.header.id = PKT_S_ZONE_LEAVE_COMPLETED;
                ackPkt.playerID = playerID;
                ackPkt.nextZoneID = portalData->destZoneID;

                GatewaySession::GetInstance()->SendToClient(
                    sessionID, self->zoneID_, self->channelID_, &ackPkt, sizeof(ackPkt));
            });
        });
}

void Zone::HandlePortalRequest(const PlayerRef& player, uint32_t portalID)
{
    if (player->IsDead())
        return;

    const PortalData* portal = PortalTemplate::Instance().Find(zoneID_, portalID);
    if (!portal)
        return; // zone에 세팅되어 있지 않은 포탈

    // 포탈 근처에서 사용한건지 체크
    float dx = player->GetX() - portal->x;
    float dy = player->GetY() - portal->y;
    float dz = player->GetZ() - portal->z;

    if (dx * dx + dy * dy + dz * dz > portal->radius * portal->radius)
        return;

    HandlePortal(player->GetPlayerID(), portal->portalID);
}

void Zone::BroadCast(const void* data, uint16_t size, uint64_t exceptPlayerID)
{
    // 현재 존의 모든 플레이어 정보
    for (auto& [id, p] : players_)
    {
        // 내 정보는 넘김
        if (id == exceptPlayerID)
            continue;

        SendTo(p, data, size);
    }
}

void Zone::SendTo(const PlayerRef& player, const void* data, uint16_t size)
{
    GatewaySession::GetInstance()->SendToClient(player->GetSessionID(), zoneID_, channelID_, data, size);
}
