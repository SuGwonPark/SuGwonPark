#pragma once
#include "pch.h"

struct SpawnPoint { float x, y, z; };

// Zone별 기본 스폰 지점 (좌표는 예시, 실제 맵에 맞게 수정)
inline std::optional<SpawnPoint> FindSpawnPoint(uint32_t zoneID) {
	switch (zoneID) {
	case 1:  return SpawnPoint{ 0.f,   0.f,  0.f };
	case 2:  return SpawnPoint{ 100.f, 0.f, 50.f };
	default: return std::nullopt;
	}
}