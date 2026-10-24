#pragma once
#include "pch.h"
#include "ServerCore/JobSystem/JobQueue.h"
#include "ServerCore/JobSystem/WorkStealingThreadPool.h" 
#include "ServerCore/JobSystem/JobSerializer.h"
#include "ServerCore/Protocol/ProtocolTypes.h"

class Player;
using PlayerRef = std::shared_ptr<Player>;

class Monster;
using MonsterRef = std::shared_ptr<Monster>;

class Zone : public JobSerializer<Zone> {
public:
	explicit Zone(uint32_t zoneID, uint32_t channelID);
	~Zone() = default;

	uint32_t GetZoneID() const { return zoneID_; }
	uint32_t GetChannelID() const { return channelID_; }

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
	void HandleGatewayLost() { SaveAllAndClear(); }      // 기존 함수는 이걸 부르도록

	// 틱 타이머
	void RequestTick();
private:
	void HandleReadyToSpawn(uint64_t sessionID, uint8_t* payload, uint16_t size);

	// 입장판 플레이어에 존의 몬스터 정보 전송
	void SendMonsterList(const PlayerRef& player);

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
	void OnMonsterDead(Monster& monster, const PlayerRef& killer);
	void RespawnMonster(Monster& monster);
	void BroadcastMonsterHp(const Monster& monster);

	void MonsterAttack(Monster& monster, const PlayerRef& target);
	void HandleRevive(const PlayerRef& player);
	void GiveRewards(const PlayerRef& player, const std::vector<RewardResult>& rewards);

	// 스킬 캐스팅
	void HandleSkillCast(const PlayerRef& caster, uint32_t skillID, ObjectType TargetType, uint64_t targetUID);
	// 스킬 실패
	void SendSkillFail(const PlayerRef& caster, uint32_t skillID, SkillFailReason reason);
	void CollectMonstersInRadius(float cx, float cy, float cz, float radius, size_t maxCount, std::vector<Monster*>& out) const;
private:
	uint32_t zoneID_;
	uint32_t channelID_;
	std::unordered_map<uint64_t, PlayerRef> players_;
	std::unordered_map<uint64_t, PlayerRef> sessionToPlayer_; // sessionID -> Player (신규)
	std::unordered_map<uint64_t, MonsterRef> monsters_; // 스폰중인 몬스터 정보
	uint64_t nextMonsterUID_ = 0;

	std::chrono::steady_clock::time_point lastTick_ = std::chrono::steady_clock::now();


	std::unordered_set<uint64_t> portalPending_;   // 포탈 저장 중인 playerID
	std::unordered_set<uint64_t> spawnPending_; // 스폰 조회 중인 SessionID
	std::atomic<bool> tickPending_{ false };
};

using ZoneRef = std::shared_ptr<Zone>;