#pragma once
#include "pch.h"

// Zone 스폰 지점 
struct SpawnPoint { float x, y, z; };
inline std::optional<SpawnPoint> FindSpawnPoint(uint32_t zoneID) {
	switch (zoneID) {
	case 1:  return SpawnPoint{ 0.f,   0.f,  0.f };
	case 2:  return SpawnPoint{ 100.f, 0.f, 50.f };
	default: return std::nullopt;
	}
}

// 포탈 위치 정보
struct PortalData {
	uint32_t portalID;
	uint32_t zoneID;         // 포탈이 놓인 Zone
	float    x, y, z;        // 포탈 위치
	float    radius;         // 이 거리 안에서만 사용 가능
	uint32_t destZoneID;     // 목적지
};

inline const PortalData* FindPortal(uint32_t zoneID, uint32_t portalID) {
	// 임시 데이터. 나중에 Data 폴더의 파일에서 읽도록 바꾼다
	static const PortalData portals[] = {
		{ 1, 1,  50.f, 0.f, 50.f, 5.f, 2 },   // Zone1 → Zone2
		{ 2, 2,  80.f, 0.f, 30.f, 5.f, 1 },   // Zone2 → Zone1
	};
	for (const auto& p : portals) {
		if (p.portalID == portalID && p.zoneID == zoneID) return &p;
	}
	return nullptr;
}