#include "pch.h"
#include "Template/ZoneTemplate.h"
#include "ServerCore/Utils/CsvReader.h"

bool ZoneTemplate::Load()
{
	data_.clear();
	return Csv::ForEachRow("Zone.csv", [this](const CsvRow& r) {
		ZoneData d{};
		d.zoneID = Csv::ToU32(r.at(0));
		d.channelCount = Csv::ToU32(r.at(1));
		d.maxPlayer = Csv::ToU32(r.at(2));
		Csv::ToVec3(r.at(3), d.spawnX, d.spawnY, d.spawnZ);

		if (d.channelCount == 0) throw std::invalid_argument("ChannelCount는 1 이상");
		if (d.maxPlayer == 0)    throw std::invalid_argument("MaxPlayer는 1 이상");
		if (!data_.emplace(d.zoneID, d).second) throw std::invalid_argument("중복 ID " + r.at(0));
		});
}

const ZoneData* ZoneTemplate::GetData(uint32_t zoneID) const
{
	auto it = data_.find(zoneID);
	return it == data_.end() ? nullptr : &it->second;
}
