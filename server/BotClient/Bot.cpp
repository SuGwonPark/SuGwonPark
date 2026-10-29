#include "Bot.h"
#include "BotStats.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

namespace
{
constexpr float CHASE_RANGE = 15.f;                              // 이 안의 몬스터를 쫓아간다
constexpr float ATTACK_RANGE = 2.5f;                             // 서버 PLAYER_ATTACK_RANGE
constexpr float ATTACK_STOP_DIST = 2.f;                          // 몬스터 앞 이 거리에서 멈춤
constexpr auto ATTACK_INTERVAL = std::chrono::milliseconds(600); // 서버 쿨타임 500 + 여유
constexpr auto SKILL_INTERVAL = std::chrono::milliseconds(1600); // Slash 쿨타임 1500 + 여유
constexpr auto REVIVE_INTERVAL = std::chrono::seconds(3);
constexpr uint32_t SKILL_ID = 1; // Slash (Single, 사거리 2.5, MP 5)
constexpr int32_t SKILL_COST = 5;
constexpr int MAX_CLOSE_LOG = 20; // 종료 로그는 앞의 20개만 (봇 수백 개면 화면이 넘침)

BotStats& Stats()
{
    return BotStats::Get();
}

template <size_t N>
void CopyStr(char (&dst)[N], const std::string& src)
{
    const size_t n = std::min(src.size(), N - 1);
    std::memcpy(dst, src.data(), n);
    dst[n] = '\0';
}

// 크기가 모자라면 nullptr (잘린 패킷 방어)
template <typename T>
const T* As(const uint8_t* data, uint16_t size)
{
    return size >= sizeof(T) ? reinterpret_cast<const T*>(data) : nullptr;
}

float DistSq(float ax, float az, float bx, float bz)
{
    const float dx = ax - bx, dz = az - bz;
    return dx * dx + dz * dz;
}
} // namespace

Bot::Bot(net::io_context& io, const BotConfig& cfg, int index)
    : cfg_(cfg), index_(index), accountID_(cfg.idPrefix + std::to_string(index)), strand_(net::make_strand(io)),
      socket_(strand_), tick_(strand_), rng_(std::random_device{}() ^ static_cast<uint32_t>(index * 7919))
{
}

void Bot::Start()
{
    auto self = shared_from_this();
    net::post(strand_, [this, self]() {
        boost::system::error_code ec;
        const auto addr = net::ip::make_address(cfg_.host, ec);
        if (ec)
            return Close("잘못된 host");

        socket_.async_connect(tcp::endpoint(addr, cfg_.port), [this, self](const boost::system::error_code& ec) {
            if (ec)
                return Close("접속 실패");

            ++Stats().connected;
            countedConnected_ = true;

            boost::system::error_code ignore;
            socket_.set_option(tcp::no_delay(true), ignore);

            state_ = State::LoggingIn;
            REQ_LoginPacket pkt{};
            CopyStr(pkt.userID, accountID_);
            CopyStr(pkt.password, cfg_.password);
            SendPacket(pkt, PKT_C_LOGIN);

            DoRead();
        });
    });
}

void Bot::Stop()
{
    auto self = shared_from_this();
    net::post(strand_, [this, self]() { Close("종료 요청"); });
}

// ===================== 네트워크 =====================

void Bot::DoRead()
{
    auto self = shared_from_this();
    socket_.async_read_some(
        net::buffer(recvBuf_.data() + writePos_, recvBuf_.size() - writePos_),
        [this, self](const boost::system::error_code& ec, size_t n) {
            if (ec)
                return Close("수신 종료");

            writePos_ += n;
            Stats().recvBytes += n;
            ProcessPackets();

            if (state_ != State::Closed)
                DoRead();
        });
}

void Bot::ProcessPackets()
{
    while (writePos_ - readPos_ >= sizeof(PacketHeader))
    {
        PacketHeader header{};
        std::memcpy(&header, &recvBuf_[readPos_], sizeof(header));

        if (header.size < sizeof(PacketHeader))
            return Close("잘못된 패킷 크기");
        if (writePos_ - readPos_ < header.size)
            break; // 아직 덜 옴

        ++Stats().recvPackets;
        HandlePacket(header.id, &recvBuf_[readPos_], header.size);
        if (state_ == State::Closed)
            return;

        readPos_ += header.size;
    }

    // 남은 조각을 앞으로 당긴다 (다음 read 공간 확보)
    const size_t remain = writePos_ - readPos_;
    if (readPos_ > 0 && remain > 0)
        std::memmove(recvBuf_.data(), &recvBuf_[readPos_], remain);
    readPos_ = 0;
    writePos_ = remain;
}

void Bot::Send(const void* data, uint16_t size)
{
    if (state_ == State::Closed)
        return;

    const auto* bytes = static_cast<const uint8_t*>(data);
    sendQueue_.emplace_back(bytes, bytes + size);
    ++Stats().sentPackets;
    Stats().sentBytes += size;

    if (sendQueue_.size() == 1)
        DoWrite();
}

void Bot::DoWrite()
{
    auto self = shared_from_this();
    net::async_write(
        socket_, net::buffer(sendQueue_.front()), [this, self](const boost::system::error_code& ec, size_t) {
            if (ec)
                return Close("송신 실패");

            sendQueue_.pop_front();
            if (!sendQueue_.empty())
                DoWrite();
        });
}

void Bot::Close(const char* reason)
{
    if (state_ == State::Closed)
        return;

    if (state_ == State::InGame)
        --Stats().inGame;
    if (countedConnected_)
        --Stats().connected;
    state_ = State::Closed;

    if (++Stats().closed <= MAX_CLOSE_LOG)
        std::cout << "[" << accountID_ << "] 종료: " << reason << std::endl;

    boost::system::error_code ec;
    tick_.cancel();
    socket_.shutdown(tcp::socket::shutdown_both, ec);
    socket_.close(ec);
}

// ===================== 패킷 처리 =====================

void Bot::HandlePacket(uint16_t id, const uint8_t* data, uint16_t size)
{
    switch (id)
    {
        // ---------- 로그인 / 로비 ----------
        case PKT_S_LOGIN: {
            const auto* p = As<RES_LoginPacket>(data, size);
            if (!p)
                return;
            if (!p->success)
            {
                ++Stats().loginFail;
                return Close("로그인 실패");
            }
            state_ = State::Lobby;
            REQ_CharacterListPacket req{};
            SendPacket(req, PKT_C_CHARACTER_LIST);
            return;
        }
        case PKT_S_CHARACTER_LIST: {
            const auto* p = As<RES_CharacterListPacket>(data, size);
            if (!p)
                return;
            if (p->count == 0 || size < sizeof(*p) + sizeof(CharacterSummary))
                return CreateCharacter();

            CharacterSummary first{};
            std::memcpy(&first, data + sizeof(*p), sizeof(first));
            return SelectCharacter(first.playerID);
        }
        case PKT_S_CREATE_CHARACTER: {
            const auto* p = As<RES_CreateCharacterPacket>(data, size);
            if (!p)
                return;
            if (p->result != CreateCharacterResult::Success)
            {
                ++Stats().createFail;
                return Close("캐릭터 생성 실패");
            }
            return SelectCharacter(p->character.playerID);
        }
        case PKT_S_SELECT_CHARACTER: {
            const auto* p = As<RES_SelectCharacterPacket>(data, size);
            if (!p)
                return;
            if (!p->success)
            {
                ++Stats().selectFail;
                return Close("캐릭터 선택 실패");
            }
            state_ = State::Spawning;
            REQ_ReadyToSpawnPacket req{}; // playerID는 Gateway가 덮어씀
            SendPacket(req, PKT_C_READY_TO_SPAWN);
            return;
        }

        // ---------- 입장 ----------
        case PKT_S_ENTER_GAME: {
            const auto* p = As<RES_EnterGamePacket>(data, size);
            if (!p || state_ != State::Spawning)
                return;
            myID_ = p->me.playerID;
            x_ = p->me.x;
            y_ = p->me.y;
            z_ = p->me.z;
            hp_ = p->me.hp;
            mp_ = p->me.mp;

            state_ = State::InGame;
            ++Stats().inGame;
            PickWaypoint();
            ScheduleTick();
            return;
        }

        // ---------- 몬스터 ----------
        case PKT_S_MONSTER_SPAWN: {
            const auto* p = As<RES_MonsterSpawnPacket>(data, size);
            if (!p || size < sizeof(*p) + p->count * sizeof(MonsterInfo))
                return;
            for (uint16_t i = 0; i < p->count; ++i)
            {
                MonsterInfo info{};
                std::memcpy(&info, data + sizeof(*p) + i * sizeof(MonsterInfo), sizeof(info));
                monsters_[info.monsterUID] = MonsterView{info.x, info.z, info.hp};
            }
            return;
        }
        case PKT_S_MONSTER_DESPAWN: {
            if (const auto* p = As<RES_MonsterDespawnPacket>(data, size))
                monsters_.erase(p->monsterUID);
            return;
        }
        case PKT_S_MONSTER_MOVE: {
            const auto* p = As<RES_MonsterMovePacket>(data, size);
            if (!p)
                return;
            auto it = monsters_.find(p->monsterUID);
            if (it != monsters_.end())
            {
                it->second.x = p->x;
                it->second.z = p->z;
            }
            return;
        }
        case PKT_S_MONSTER_HP: {
            const auto* p = As<RES_MonsterHpPacket>(data, size);
            if (!p)
                return;
            auto it = monsters_.find(p->monsterUID);
            if (it != monsters_.end())
                it->second.hp = p->hp;
            return;
        }

        // ---------- 전투 / 스탯 ----------
        case PKT_S_ATTACK: {
            const auto* p = As<RES_AttackPacket>(data, size);
            if (!p)
                return;
            if (p->targetType == ObjectType::Player && p->targetUID == myID_)
            {
                if (hp_ > 0 && p->targetHp <= 0)
                    ++Stats().deaths;
                hp_ = p->targetHp;
            }
            else if (p->targetType == ObjectType::Monster)
            {
                auto it = monsters_.find(p->targetUID);
                if (it != monsters_.end())
                    it->second.hp = p->targetHp;
            }
            return;
        }
        case PKT_S_SKILL_CAST: {
            const auto* p = As<RES_SkillCastPacket>(data, size);
            if (p && p->casterID == myID_)
                mp_ = p->casterMp;
            return;
        }
        case PKT_S_SKILL_FAIL: {
            ++Stats().skillFails;
            return;
        }
        case PKT_S_PLAYER_STAT: {
            const auto* p = As<RES_PlayerStatPacket>(data, size);
            if (p && p->playerID == myID_)
            {
                hp_ = p->hp;
                mp_ = p->mp;
            }
            return;
        }
        case PKT_S_REVIVE: {
            const auto* p = As<RES_RevivePacket>(data, size);
            if (p && p->playerID == myID_)
            {
                x_ = p->x;
                y_ = p->y;
                z_ = p->z;
                hp_ = p->hp;
                PickWaypoint();
            }
            return;
        }
        case PKT_S_MOVE: {
            // 다른 사람 이동은 무시. 내 ID로 오면 서버가 위치를 되돌린 것
            const auto* p = As<MovePacket>(data, size);
            if (p && p->playerID == myID_)
            {
                ++Stats().moveCorrections;
                x_ = p->x;
                y_ = p->y;
                z_ = p->z;
            }
            return;
        }
        default:
            return; // S_SPAWN, S_DESPAWN, S_EXP 등은 받기만 (통계에는 잡힘)
    }
}

void Bot::SelectCharacter(uint64_t playerID)
{
    state_ = State::Selecting;
    REQ_SelectCharacterPacket req{};
    req.playerID = playerID;
    SendPacket(req, PKT_C_SELECT_CHARACTER);
}

void Bot::CreateCharacter()
{
    REQ_CreateCharacterPacket req{};
    CopyStr(req.name, accountID_); // 이름 = 계정 id (2~12자 영문/숫자)
    SendPacket(req, PKT_C_CREATE_CHARACTER);
}

// ===================== 행동 =====================

void Bot::ScheduleTick()
{
    tick_.expires_after(std::chrono::milliseconds(cfg_.tickMs));
    auto self = shared_from_this();
    tick_.async_wait([this, self](const boost::system::error_code& ec) {
        if (ec || state_ != State::InGame)
            return;
        OnTick();
        ScheduleTick();
    });
}

void Bot::OnTick()
{
    const auto now = Clock::now();

    // 죽었으면 부활 요청만
    if (hp_ <= 0)
    {
        if (now >= nextReviveAt_)
        {
            REQ_RevivePacket req{};
            SendPacket(req, PKT_C_REVIVE);
            nextReviveAt_ = now + REVIVE_INTERVAL;
        }
        return;
    }

    const float step = cfg_.moveSpeed * cfg_.tickMs / 1000.f;

    // 가장 가까운 몬스터
    uint64_t targetUID = 0;
    float bestSq = CHASE_RANGE * CHASE_RANGE;
    float tx = 0.f, tz = 0.f;
    for (const auto& [uid, m] : monsters_)
    {
        if (m.hp <= 0)
            continue;
        const float d = DistSq(x_, z_, m.x, m.z);
        if (d < bestSq)
        {
            bestSq = d;
            targetUID = uid;
            tx = m.x;
            tz = m.z;
        }
    }

    if (targetUID == 0)
    {
        // 배회: 목표 지점에 도착하면 새 지점
        if (DistSq(x_, z_, wx_, wz_) < 1.f)
            PickWaypoint();
        MoveToward(wx_, wz_, step, 0.f);
        return;
    }

    if (bestSq > ATTACK_RANGE * ATTACK_RANGE)
    {
        MoveToward(tx, tz, step, ATTACK_STOP_DIST);
        return;
    }

    // 사거리 안: 스킬(MP 있고 쿨 지났으면) 아니면 평타
    if (mp_ >= SKILL_COST && now >= nextSkillAt_)
    {
        REQ_SkillCastPacket req{};
        req.skillID = SKILL_ID;
        req.targetType = ObjectType::Monster;
        req.targetUID = targetUID;
        SendPacket(req, PKT_C_SKILL_CAST);
        nextSkillAt_ = now + SKILL_INTERVAL;
    }
    else if (now >= nextAttackAt_)
    {
        REQ_AttackPacket req{};
        req.targetType = ObjectType::Monster;
        req.targetUID = targetUID;
        SendPacket(req, PKT_C_ATTACK);
        nextAttackAt_ = now + ATTACK_INTERVAL;
    }
}

void Bot::MoveToward(float tx, float tz, float step, float stopDist)
{
    const float dx = tx - x_, dz = tz - z_;
    const float dist = std::sqrt(dx * dx + dz * dz);
    const float move = std::min(step, dist - stopDist);
    if (move <= 0.01f)
        return;

    x_ += dx / dist * move;
    z_ += dz / dist * move;

    MovePacket pkt{};
    pkt.playerID = myID_;
    pkt.x = x_;
    pkt.y = y_;
    pkt.z = z_;
    SendPacket(pkt, PKT_C_MOVE);
}

void Bot::PickWaypoint()
{
    std::uniform_real_distribution<float> d(-cfg_.areaHalf, cfg_.areaHalf);
    wx_ = d(rng_);
    wz_ = d(rng_);
}