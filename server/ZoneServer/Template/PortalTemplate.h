#pragma once
#include <cstdint>
#include <unordered_map>

// Data/Portal.csv : ID,ZoneID,Position,Radius,DestZoneID,DestSpawnPosition
struct PortalData {
	uint32_t portalID;                 // 전체에서 유일
	uint32_t zoneID;                   // 포탈이 놓인 존
	float    x, y, z;                  // 포탈 위치
	float    radius;                   // 이 거리 안에서만 사용 가능
	uint32_t destZoneID;               // 도착 존
	float    destX, destY, destZ;      // 도착 존에서 나타날 위치
};

class PortalTemplate {
public:
	static PortalTemplate& Instance() { static PortalTemplate inst; return inst; }

	bool Load();
	bool Validate() const;

	const PortalData* GetData(uint32_t portalID) const;                  // 없으면 nullptr
	const std::vector<PortalData>& GetByZone(uint32_t zoneID);
	const PortalData* Find(uint32_t zoneID, uint32_t portalID) const;    // 다른 존의 포탈이면 nullptr
	const std::unordered_map<uint32_t, PortalData>& All() const { return data_; }

private:
	std::unordered_map<uint32_t, PortalData> data_;
	std::unordered_map<uint32_t, std::vector<PortalData>> byZone_;
};
