#include "pch.h"
#include "Template/Template.h"
#include "Template/ZoneTemplate.h"
#include "Template/PortalTemplate.h"
#include "Template/MonsterTemplate.h"
#include "Template/MonsterSpawnTemplate.h"
#include "Template/RewardTemplate.h"
#include "Template/SkillTemplate.h"

bool Template::LoadTemplate()
{
	try {
		// 테이블 하나씩 로드. 
		if (!ZoneTemplate::Instance().Load())    return false;
		if (!PortalTemplate::Instance().Load())  return false;
		if (!MonsterTemplate::Instance().Load()) return false;
		if (!MonsterSpawnTemplate::Instance().Load()) return false;
		if (!RewardTemplate::Instance().Load()) return false;
		if (!SkillTemplate::Instance().Load()) return false;

		// 2) 테이블끼리 참조 검증 (Portal → Zone)
		if (!PortalTemplate::Instance().Validate()) return false;
		if (!MonsterSpawnTemplate::Instance().Validate()) return false;
		if (!RewardTemplate::Instance().Validate()) return false;
	}
	catch (const std::exception& e) {
		std::cerr << "[Template] 로드 중 예외: " << e.what() << std::endl;
		return false;
	}

	std::cout << "[Template] 로드 완료 (Zone " << ZoneTemplate::Instance().All().size()
		<< ", Portal " << PortalTemplate::Instance().All().size()
		<< ", Monster " << MonsterTemplate::Instance().All().size() << ")" << std::endl;
	return true;
}
