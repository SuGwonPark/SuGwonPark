#include "pch.h"
#include "Template/ZoneTemplate.h"
#include "ServerCore/Utils/CsvReader.h"

bool ZoneTemplate::Load()
{
	data_.clear();
	return Csv::ForEachRow("Zone.csv", [this](const CsvRowView& r) {
		ZoneData d{};
		d.zoneID = r.U32("ID");
		d.channelCount = r.U32("ChannelCount");
		d.maxPlayer = r.U32("MaxPlayer");
		r.Vec3("SpawnPosition", d.spawnX, d.spawnY, d.spawnZ);

		if (d.channelCount == 0) throw std::invalid_argument("ChannelCount는 1 이상");
		if (d.maxPlayer == 0)    throw std::invalid_argument("MaxPlayer는 1 이상");
		if (!data_.emplace(d.zoneID, d).second) throw std::invalid_argument("중복 ID " + r.Str("ID"));
		});
}

const ZoneData* ZoneTemplate::GetData(uint32_t zoneID) const
{
	auto it = data_.find(zoneID);
	return it == data_.end() ? nullptr : &it->second;
}
