#pragma once
#include "iostream"

namespace ZoneConst {
	inline constexpr float MAX_MOVE_SPEED = 10.f;

	inline constexpr int   ZONE_TICK_MS = 100;             // 존 업데이트 주기 (10Hz)
	inline constexpr float MONSTER_LEASH_RANGE = 30.f;     // 스폰 지점에서 이 이상 끌려가면 귀환
	inline constexpr float MONSTER_ARRIVE_EPSILON = 0.1f;  // 이 거리 안이면 도착으로 봄
	inline constexpr float MAX_TICK_DT = 0.5f;

	// 플레이어 기본 공격 (임시)
	inline constexpr int32_t PLAYER_ATTACK_DAMAGE = 10; // 캐릭터 데미지
	inline constexpr float   PLAYER_ATTACK_RANGE = 2.5f; // 캐릭터 공격 범위
	inline constexpr int     PLAYER_ATTACK_COOLDOWN_MS = 500;
	inline constexpr float   ATTACK_RANGE_TOLERANCE = 1.0f;   // 지연으로 인한 위치 오차 허용

	inline constexpr int     MONSTER_ATTACK_COOLDOWN_MS = 1500;

	inline constexpr size_t  SKILL_MAX_TARGETS = 8;            // 범위 스킬 최대 타격 수
}
