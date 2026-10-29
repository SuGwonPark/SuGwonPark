#include "pch.h"
#include "Template/Template.h"
#include "Template/ZoneTemplate.h"
#include "Template/PortalTemplate.h"
#include "Template/MonsterTemplate.h"
#include "Template/MonsterSpawnTemplate.h"
#include "Template/RewardTemplate.h"
#include "Template/SkillTemplate.h"
#include "Template/LevelTemplate.h"
#include "Template/StatTemplate.h"

bool Template::LoadTemplate()
{
    try
    {
        ZoneTemplate::Instance().Load();
        PortalTemplate::Instance().Load();
        MonsterTemplate::Instance().Load();
        MonsterSpawnTemplate::Instance().Load();
        RewardTemplate::Instance().Load();
        SkillTemplate::Instance().Load();
        LevelTemplate::Instance().Load();
        StatTemplate::Instance().Load();

        // 2) 테이블끼리 참조 검증 (Portal → Zone)
        PortalTemplate::Instance().Validate();
        MonsterSpawnTemplate::Instance().Validate();
        RewardTemplate::Instance().Validate();
        StatTemplate::Instance().Validate();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Template] 로드 중 예외: " << e.what() << std::endl;
        return false;
    }

    std::cout << "[Template] 로드 완료 (Zone " << ZoneTemplate::Instance().All().size() << ", Portal "
              << PortalTemplate::Instance().All().size() << ", Monster " << MonsterTemplate::Instance().All().size()
              << ")" << std::endl;
    return true;
}
