#pragma once
#include "ServerCore/JobSystem/JobSerializer.h"
#include "ServerCore/Protocol/ProtocolTypes.h"
#include "Game/AoiGrid.h"

class Player;
using PlayerRef = std::shared_ptr<Player>;

class Monster;
using MonsterRef = std::shared_ptr<Monster>;

struct RewardResult;
struct SkillEffectData;

struct DropItem
{
    uint64_t dropUID;
    uint32_t itemID;
    uint32_t count;
    float x, y, z;
    uint64_t ownerID;                                 // 우선권 (0 = 누구나)
    std::chrono::steady_clock::time_point ownerUntil; // 이 시각 전까지는 ownerID만 주울 수 있음
    std::chrono::steady_clock::time_point despawnAt;  // 이 시각에 사라짐
};

class Zone : public JobSerializer<Zone>
{
  public:
    explicit Zone(uint32_t zoneID, uint32_t channelID);
    ~Zone() = default;

    uint32_t GetZoneID() const
    {
        return zoneID_;
    }
    uint32_t GetChannelID() const
    {
        return channelID_;
    }

    // 서버 시작 시 1회 Zone에 몬스터 정보 세팅
    void InitMonster();

    // 존 입장
    void Enter(PlayerRef player);
    // 존 퇴장
    void Leave(uint64_t playerId);

    // 종료
    void HandleDisconnect(uint64_t sessionID);

    void HandleClientPacket(uint64_t sessionID, uint8_t* payload, uint16_t size);
    // 캐릭터 무브
    void HandleMove(uint64_t playerId, float x, float y, float z);

    // 존 이동
    void HandlePortal(uint64_t playerId, uint32_t portalID);

    // 포탈 이동
    void HandlePortalRequest(const PlayerRef& player, uint32_t portalID);

    // 플레이어에 전달
    void SendTo(const PlayerRef& player, const void* data, uint16_t size);
    // 모든 플레이어에 전달
    void BroadCast(const void* data, uint16_t size, uint64_t exceptPlayerID = 0);

    // 모든 플레이어 저장 요청 후 Zone 비우기. 끝나면 onDone 호출 (Zone 스레드에서)
    void SaveAllAndClear(std::function<void()> onDone = nullptr);
    void HandleGatewayLost()
    {
        SaveAllAndClear();
    } // 기존 함수는 이걸 부르도록

    // 틱 타이머
    void RequestTick();
    void Shutdown(std::function<void()> onDone);

  private:
    void HandleReadyToSpawn(uint64_t sessionID, uint8_t* payload, uint16_t size);

    // 실시간 업데이트
    void Update();

    // 몬스터 행동 업데이트
    void UpdateMonster(Monster& monster, float dt, std::chrono::steady_clock::time_point now);
    // 가까운 플레이어 탐색
    PlayerRef FindNearestPlayer(const Monster& monster, float range) const;
    // 클라이언트에 전송
    void BroadcastMonsterMove(const Monster& monster);

    // 전투
    void HandleAttack(const PlayerRef& attacker, ObjectType targetType, uint64_t targetUID);
    void OnMonsterDead(Monster& monster);
    void RespawnMonster(Monster& monster);
    void BroadcastMonsterHp(const Monster& monster);

    void MonsterAttack(Monster& monster, const PlayerRef& target);
    void HandleRevive(const PlayerRef& player);

    // 보상 지급
    void GiveRewards(const PlayerRef& player, const std::vector<RewardResult>& rewards);
    void DistributeRewards(const Monster& monster);

    // 스킬 캐스팅
    void HandleSkillCast(const PlayerRef& caster, uint32_t skillID, ObjectType TargetType, uint64_t targetUID);
    // 스킬 실패
    void SendSkillFail(const PlayerRef& caster, uint32_t skillID, SkillFailReason reason);

    // 몬스터 1마리 효과 적용 (데미지, 회복, 버프, 디버프 등)
    int32_t ApplySkillEffect(
        const PlayerRef& caster,
        const std::vector<SkillEffectData>& effects,
        Monster& monster,
        std::chrono::steady_clock::time_point now);
    // 몬스터 지속 효과 (디버프 도트) 처리
    bool UpdateMonsterEffects(Monster& monster, std::chrono::steady_clock::time_point now);

    void CollectMonstersInRadius(
        float cx, float cy, float cz, float radius, size_t maxCount, std::vector<Monster*>& out) const;

    void RegenPlayers();

    void SendPlayerStat(const PlayerRef& to, const PlayerRef& player);
    void AutoSaveTick(); // 자동 저장 처리

    // 부하 테스트용
    void RecordTickStat(std::chrono::steady_clock::time_point tickStart, double gapMs, float dt);

    // AOI (시야)
    AoiCell CellOf(const Player& player) const;
    // 범위 내 플레이어 수집
    void CollectPlayers(const std::vector<AoiCell>& cells, uint64_t exceptPlayerID, std::vector<PlayerRef>& out) const;
    // targets에 있는 플레이어에게 data 전송
    void MulticastTo(const std::vector<PlayerRef>& targets, const void* data, uint16_t size);
    // 범위 내 모든 유저
    void BroadcastNear(AoiCell center, const void* data, uint16_t size, uint64_t exceptPlayerID = 0);
    // 범위 내 플레이어에게만 전송
    void SendSpawns(const PlayerRef& to, const std::vector<PlayerRef>& list);           // list → to 에게 등장
    void SpawnPlayerTo(const PlayerRef& player, const std::vector<PlayerRef>& viewers); // player → viewers 에게 등장
    void DespawnPlayerTo(uint64_t playerID, const std::vector<PlayerRef>& viewers);
    void OnPlayerCellChanged(const PlayerRef& player, AoiCell from, AoiCell to);

    // AOI (몬스터)
    AoiCell CellOf(const Monster& monster) const;
    void CollectMonsters(const std::vector<AoiCell>& cells, std::vector<Monster*>& out) const;
    void SendMonsterSpawns(const PlayerRef& to, const std::vector<Monster*>& list);
    void DespawnMonstersTo(const PlayerRef& to, const std::vector<Monster*>& list); // list → to 에게서 퇴장
    void OnMonsterCellChanged(Monster& monster, AoiCell from, AoiCell to);
    // 몬스터 위치는 반드시 이 함수로만 바꾼다 (격자 동기화)
    bool MoveMonsterToward(Monster& monster, float tx, float ty, float tz, float step, float stopDist);

    // 필드 아이템 드랍
    void SpawnDrop(
        uint32_t itemID,
        uint32_t count,
        float x,
        float y,
        float z,
        uint64_t ownerID,
        std::chrono::steady_clock::time_point now);

    // 드랍 정보 전달
    void UpdateDrops(std::chrono::steady_clock::time_point now);
    // 드랍된 아이템 삭제
    void RemoveDrop(uint64_t dropUID);
    // 드롭 아이템 획득
    void HandlePickup(const PlayerRef& player, uint64_t dropUID);
    // 드롭 정보 플레이어에 전달
    void SendDropSpawns(
        const PlayerRef& to, const std::vector<AoiCell>& cells, std::chrono::steady_clock::time_point now);
    // 드롭 삭제 정보 플레이어에 전달
    void SendDropDespawns(const PlayerRef& to, const std::vector<AoiCell>& cells);

    // 인벤토리 바뀐 칸: 중복 제거 → DB 저장 → 클라 알림 (보상 / 줍기 공용)
    void CommitInventoryChange(const PlayerRef& player, std::vector<uint16_t>& changedSlots);

  private:
    uint32_t zoneID_;
    uint32_t channelID_;
    std::unordered_map<uint64_t, PlayerRef> players_;
    std::unordered_map<uint64_t, PlayerRef> sessionToPlayer_; // sessionID -> Player (신규)
    std::unordered_map<uint64_t, MonsterRef> monsters_;       // 스폰중인 몬스터 정보
    uint64_t nextMonsterUID_ = 0;

    std::chrono::steady_clock::time_point lastTick_ = std::chrono::steady_clock::now();

    std::unordered_set<uint64_t> portalPending_; // 포탈 저장 중인 playerID
    std::unordered_set<uint64_t> spawnPending_;  // 스폰 조회 중인 SessionID
    std::atomic<bool> tickPending_{false};

    float regenTimer_ = 0.f;
    float autoSaveTimer_ = 0.f;
    uint32_t autoSaveSlice_ = 0;

    bool closed_ = false; // 존 종료 중

    // AOI 시야 범위
    AoiGrid playerGrid_;
    AoiGrid monsterGrid_;
    AoiGrid dropGrid_;

    std::unordered_map<uint64_t, DropItem> drops_;
    uint64_t nextDropUID_ = 0;

    // 틱 측정 (부하 테스트용)
    float tickStatTimer_ = 0.f;
    uint32_t tickStatCount_ = 0;
    int64_t tickWorkSumUs_ = 0; // Update 처리 시간 합 (마이크로초)
    int64_t tickWorkMaxUs_ = 0;
    double tickGapMaxMs_ = 0.0; // 틱 사이 실제 간격 최대 (100ms보다 많이 크면 존이 밀리는 중)
};

using ZoneRef = std::shared_ptr<Zone>;