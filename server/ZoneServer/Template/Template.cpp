#include "pch.h"
#include "Template/Template.h"
#include "Template/ZoneTemplate.h"
#include "Template/PortalTemplate.h"
#include "Template/MonsterTemplate.h"
#include "Template/MonsterSpawnTemplate.h"
#include "Template/RewardTemplate.h"
#include "Template/SkillTemplate.h"
#include "Template/SkillEffectTemplate.h"
#include "Template/LevelTemplate.h"
#include "Template/StatTemplate.h"
#include "Template/ItemTemplate.h"
#include "Template/ItemOptionTemplate.h"
#include "Template/ItemSubOptionTemplate.h"

#include "ServerCore/Template/CharacterTemplate.h"

bool Template::LoadTemplate()
{
    try
    {
        if (!ZoneTemplate::Instance().Load())
            return false;
        if (!PortalTemplate::Instance().Load())
            return false;
        if (!MonsterTemplate::Instance().Load())
            return false;
        if (!MonsterSpawnTemplate::Instance().Load())
            return false;
        if (!RewardTemplate::Instance().Load())
            return false;
        if (!SkillEffectTemplate::Instance().Load())
            return false;
        if (!SkillTemplate::Instance().Load())
            return false;
        if (!LevelTemplate::Instance().Load())
            return false;
        if (!StatTemplate::Instance().Load())
            return false;
        if (!CharacterTemplate::Instance().Load())
            return false;
        if (!ItemTemplate::Instance().Load())
            return false;
        if (!ItemOptionTemplate::Instance().Load())
            return false;
        if (!ItemSubOptionTemplate::Instance().Load())
            return false;

        // 2) 테이블끼리 참조 검증
        if (!PortalTemplate::Instance().Validate())
            return false;
        if (!MonsterSpawnTemplate::Instance().Validate())
            return false;
        if (!RewardTemplate::Instance().Validate())
            return false;
        if (!SkillTemplate::Instance().Validate())
            return false;
        if (!StatTemplate::Instance().Validate())
            return false;
        if (!ItemTemplate::Instance().Validate())
            return false;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Template] 로드 중 예외: " << e.what() << std::endl;
        return false;
    }
    catch (...)
    {
        std::cerr << "[Template] 로드 중 알 수 없는 예외" << std::endl;
        return false;
    }

    return true;
}
