#include "pch.h"
#include "ServerCore/Template/CharacterTemplate.h"
#include "ServerCore/Utils/CsvReader.h"
#include "ServerCore/Utils/util.h"

// Data/Character.csv : ID,Gender,ClassType  (캐릭터 생성 때 고를 수 있는 외형 목록)
bool CharacterTemplate::Load()
{
	data_.clear();
	return Csv::ForEachRow("Character.csv", [this](const CsvRowView& r) {
		CharData d{};
		const uint32_t id = r.U32("ID");
		if (id == 0 || id > 255) throw std::invalid_argument("ID는 1 ~ 255");
		d.charID = static_cast<uint8_t>(id);
		d.classType = ParseCsvEnum<ClassType>(r.Str("ClassType"), "ClassType", id);
		d.gender = ParseCsvEnum<GenderType>(r.Str("Gender"), "Gender", id);

		if (!data_.emplace(d.charID, d).second)
			throw std::invalid_argument("중복 ID " + r.Str("ID"));
	});
}

const CharData* CharacterTemplate::GetData(uint32_t charID) const
{
	auto it = data_.find(charID);
	return it == data_.end() ? nullptr : &it->second;
}
