#pragma once
#include <chrono>
#include <cstdint>
#include <mutex>
#include "Game/ZoneConstants.h"

// 서버가 직접 발급하는 아이템 고유 번호 (DB 왕복 없이 바로 사용)
//  [상위 32비트: 2026-01-01부터 지난 초][8비트: 서버 번호][하위 24비트: 같은 초 안의 순번]
//  - 서버 번호가 다르면 겹치지 않음
//  - 한 초에 1677만 개를 넘기면 다음 초를 당겨 씀 (값은 계속 커짐)
//  - 재시작이 1초 안에 끝나면 겹칠 수 있지만, 템플릿/DB 로드만으로 1초 이상 걸림
namespace ItemUID
{
inline uint64_t Next()
{
    constexpr int64_t EPOCH = 1767225600; // 2026-01-01 00:00:00 UTC
    static std::mutex m;
    static uint32_t lastSec = 0;
    static uint32_t seq = 0;

    const int64_t now =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const uint32_t sec = static_cast<uint32_t>(now - EPOCH);

    std::lock_guard<std::mutex> lock(m);
    if (sec > lastSec)
    {
        lastSec = sec; // 새 초 → 순번 0부터
        seq = 0;
    }
    else if (++seq >= (1u << 24))
    {
        ++lastSec; // 같은 초(또는 시계가 뒤로 감)에 순번을 다 씀 → 다음 초로
        seq = 0;
    }
    return (static_cast<uint64_t>(lastSec) << 32) | (static_cast<uint64_t>(ZoneConst::ZONE_SERVER_ID) << 24) | seq;
}
} // namespace ItemUID