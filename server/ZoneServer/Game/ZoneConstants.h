#pragma once
#include "iostream"

namespace ZoneConst {
	inline constexpr float MAX_MOVE_SPEED = 10.f;

	inline constexpr int   ZONE_TICK_MS = 100;             // 존 업데이트 주기 (10Hz)
	inline constexpr float MONSTER_LEASH_RANGE = 30.f;     // 스폰 지점에서 이 이상 끌려가면 귀환
	inline constexpr float MONSTER_ARRIVE_EPSILON = 0.1f;  // 이 거리 안이면 도착으로 봄
	inline constexpr float MAX_TICK_DT = 0.5f;

	inline constexpr float   ATTACK_RANGE_TOLERANCE = 1.0f;   // 지연으로 인한 위치 오차 허용

	inline constexpr float   PLAYER_REGEN_INTERVAL_SEC = 1.0f; // 자동 회복 시 간격
    inline constexpr float AOI_CELL_SIZE = 20.f;             // AOI 셀 크기 (20m)

	// 자동 저장 플레이어 1/AutoSave_SLICES 만큼 나눠서 저장.
	inline constexpr float AUTOSAVE_TICK_SEC = 10.f;
    inline constexpr uint32_t AUTOSAVE_SLICES = 6;

	// 임시 데이터들 나중에 템플릿으로 뺴던가 해야함
	inline constexpr int32_t PLAYER_HP_REGEN = 1;              // 회복 1회당 (임시)
	inline constexpr int32_t PLAYER_MP_REGEN = 2;			// 임시

	inline constexpr size_t  SKILL_MAX_TARGETS = 8;            // 범위 스킬 최대 타격 수 (임시)
	inline constexpr int     MONSTER_ATTACK_COOLDOWN_MS = 1500;				// 임시

	inline constexpr int32_t PLAYER_ATTACK_DAMAGE = 10; // 캐릭터 데미지			(임시)
	inline constexpr float   PLAYER_ATTACK_RANGE = 2.5f; // 캐릭터 공격 범위		(임시)
	inline constexpr int     PLAYER_ATTACK_COOLDOWN_MS = 500;					//(임시)



		inline constexpr float TICK_STAT_INTERVAL_SEC = 5.f; // 틱 측정 출력 주기 (부하 테스트용)

    }
