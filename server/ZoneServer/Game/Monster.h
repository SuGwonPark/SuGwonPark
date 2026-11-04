#pragma once
#include "Template/MonsterTemplate.h"
#include "ServerCore/Protocol/VarEnum.h"

enum class MonsterState : uint8_t
{
    Idle,   // 스폰 위치에서 대기. 어그로 범위 안 플레이어를 찾는다
    Chase,  // 타겟 플레이어 추적
    Return, // 스폰 위치로 귀환 (귀환 중에는 어그로를 받지 않음)
    Dead,   // 사망
};

class Monster
{
  public:
    using Clock = std::chrono::steady_clock;
    Monster(uint64_t monsterUID, const MonsterData* data, float x, float y, float z);

    uint64_t GetMonsterUID() const
    {
        return monsterUID_;
    }
    const MonsterData& GetData() const
    {
        return *data_;
    }
    uint32_t GetMonsterID() const
    {
        return data_->monsterID;
    }

    float GetX() const
    {
        return x_;
    }
    float GetY() const
    {
        return y_;
    }
    float GetZ() const
    {
        return z_;
    }

    float GetSpawnX() const
    {
        return spawnX_;
    }
    float GetSpawnY() const
    {
        return spawnY_;
    }
    float GetSpawnZ() const
    {
        return spawnZ_;
    }

    int32_t GetHp() const
    {
        return hp_;
    }
    int32_t GetMaxHp() const
    {
        return data_->maxHp;
    }
    bool IsDead() const
    {
        return state_ == MonsterState::Dead;
    }

    MonsterState GetState() const
    {
        return state_;
    }
    uint64_t GetTargetID() const
    {
        return targetPlayerID_;
    }
    // 플레이어 타겟 지정
    void StartChase(uint64_t playerID)
    {
        state_ = MonsterState::Chase;
        targetPlayerID_ = playerID;
    }
    void StartReturn()
    {
        state_ = MonsterState::Return;
        targetPlayerID_ = 0;
        // 효과 초기화
        ClearEffects();
        // 기여도 초기화
        contributions_.clear();
    }

    // 기본 상태
    void SetIdle()
    {
        state_ = MonsterState::Idle;
        targetPlayerID_ = 0;
    }

    bool TryUseAttack(Clock::time_point now, std::chrono::milliseconds cooldown)
    {
        if (now - lastAttackTime_ < cooldown)
            return false;
        lastAttackTime_ = now;
        return true;
    }

    // 몬스터에게 실제로 깎은 HP 기록 (보상 우선권 계산용)
    struct Contribution
    {
        uint64_t playerID;
        int64_t damage;
    };

    const std::vector<Contribution>& GetContributions() const
    {
        return contributions_;
    }

    // ========= 전투 ===========
    // 거리 제곱
    float DistSqTo(float x, float y, float z) const;
    float DistSqFromSpawn() const
    {
        return DistSqTo(spawnX_, spawnY_, spawnZ_);
    }

    // (tx,ty,tz) 쪽으로 최대 step만큼 이동. 목표와 stopDist 거리가 되면 멈춘다. 움직였으면 true
    bool MoveToward(float tx, float ty, float tz, float step, float stopDist);

    int32_t TakeDamage(int32_t damage, uint64_t attackerID = 0);

    // 사망 처리
    void Die(Clock::time_point now);
    // 부활 가능 여부 체크
    bool CanRespawn(Clock::time_point now) const
    {
        return IsDead() && now >= respawnAt_;
    }
    // 리스폰
    void Respawn();
    bool ResetHp();

    // ====== 지속 효과 ======
    // 같은 스탯 디버프는 중첩하지 않음: 더 강하면 교체, 같으면 시간 갱신, 약하면 무시
    void AddDebuff(StatType stat, ValueType valueType, int32_t value, Clock::time_point endAt);

    // 같은 시전자의 같은 종류(DotType) 도트: 더 강하면 교체, 같으면 시간 갱신, 약하면 무시 (틱 주기 유지)
    void AddDot(
        uint64_t casterID, DotType dotType, int32_t damagePerTick, Clock::time_point now, Clock::time_point endAt);

    // 끝난 효과 제거 + 이번에 들어갈 도트 데미지 합 반환.
    int32_t UpdateEffects(Clock::time_point now);
    void ClearEffects();

    // 디버프가 반영된 이동 속도
    float GetMoveSpeed() const;
    // ====== 지속 효과 End ======

  private:
    struct Debuff
    {
        StatType stat;
        ValueType valueType;
        int32_t value;
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

  private:
    uint64_t monsterUID_;
    const MonsterData* data_; // 정적 테이블을 가리킴 (수명 걱정 없음)
    float x_, y_, z_;
    float spawnX_, spawnY_, spawnZ_;
    int32_t hp_;

    MonsterState state_ = MonsterState::Idle;
    uint64_t targetPlayerID_ = 0;

    Clock::time_point respawnAt_{};
    Clock::time_point lastAttackTime_{};
    std::vector<Debuff> debuffs_;
    std::vector<Dot> dots_;

    // 데미지 기여도
    std::vector<Contribution> contributions_{};
};

using MonsterRef = std::shared_ptr<Monster>;