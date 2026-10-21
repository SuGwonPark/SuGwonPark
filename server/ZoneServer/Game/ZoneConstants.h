#pragma once

namespace ZoneConst {
	inline constexpr float MAX_MOVE_SPEED = 10.f;

	inline constexpr int   ZONE_TICK_MS = 100;             // 존 업데이트 주기 (10Hz)
	inline constexpr float MONSTER_LEASH_RANGE = 30.f;     // 스폰 지점에서 이 이상 끌려가면 귀환
	inline constexpr float MONSTER_ARRIVE_EPSILON = 0.1f;  // 이 거리 안이면 도착으로 봄
	inline constexpr float MAX_TICK_DT = 0.5f;
}
