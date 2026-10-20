#pragma once
#include <cstdint>
#include <map>

// Data/Zone.csv : ID,ChannelCount,MaxPlayer,SpawnPosition
struct ZoneData {
	uint32_t zoneID;
	uint32_t channelCount;             // 이 존을 몇 채널로 띄울지
	uint32_t maxPlayer;                // 채널당 최대 인원
	float    spawnX, spawnY, spawnZ;   // 기본 시작 위치
};

class ZoneTemplate {
public:
	static ZoneTemplate& Instance() { static ZoneTemplate inst; return inst; }

	bool Load();
	const ZoneData* GetData(uint32_t zoneID) const;   // 없으면 nullptr
	const std::map<uint32_t, ZoneData>& All() const { return data_; }   // zoneID 순서

private:
	std::map<uint32_t, ZoneData> data_;
};
