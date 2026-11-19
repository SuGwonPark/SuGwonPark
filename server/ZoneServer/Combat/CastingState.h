#pragma once
#include <chrono>
#include "ServerCore/Protocol/Packets.h"

struct CastingInfo
{
    REQ_SkillCastPacket req;                     // 시전 요청 원본 (완료 시 재 판정)
    std::chrono::steady_clock::time_point endAt; // 완료 시각
    float startX, startZ;                        // 시전 시작 위치
};