#include "pch.h"
#include "Template/PortalTemplate.h"
#include "Template/ZoneTemplate.h"
#include "ServerCore/Utils/CsvReader.h"

bool PortalTemplate::Load()
{
	data_.clear();
	return Csv::ForEachRow("Portal.csv", [this](const CsvRowView& r) {
		PortalData d{};
		d.portalID = r.U32("ID");
		d.zoneID = r.U32("ZoneID");
		r.Vec3("Position", d.x, d.y, d.z);
		d.radius = r.Float("Radius");
		d.destZoneID = r.U32("DestZoneID");
		r.Vec3("DestSpawnPosition", d.destX, d.destY, d.destZ);

		if (d.radius <= 0.f) throw std::invalid_argument("Radius는 0보다 커야 함");
		if (!data_.emplace(d.portalID, d).second) throw std::invalid_argument("중복 ID " + r.Str("ID"));
		});
}

bool PortalTemplate::Validate() const
{
	bool ok = true;
	const ZoneTemplate& zones = ZoneTemplate::Instance();

	for (const auto& [id, p] : data_) {
		if (!zones.GetData(p.zoneID)) {
			std::cerr << "[Template] Portal.csv ID " << id << ": Zone.csv에 없는 ZoneID " << p.zoneID << std::endl;
			ok = false;
		}
		if (!zones.GetData(p.destZoneID)) {
			std::cerr << "[Template] Portal.csv ID " << id << ": Zone.csv에 없는 DestZoneID " << p.destZoneID << std::endl;
			ok = false;
		}
	}
	return ok;
}

const PortalData* PortalTemplate::GetData(uint32_t portalID) const
{
	auto it = data_.find(portalID);
	return it == data_.end() ? nullptr : &it->second;
}

const PortalData* PortalTemplate::Find(uint32_t zoneID, uint32_t portalID) const
{
	const PortalData* p = GetData(portalID);
	return (p && p->zoneID == zoneID) ? p : nullptr;
}
