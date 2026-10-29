#pragma once
#include <atomic>
#include <cstdint>

// 모든 봇이 같이 쓰는 통계 (여러 스레드에서 증가하므로 atomic)
struct BotStats
{
    std::atomic<int> connected{0}; // 접속 중
    std::atomic<int> inGame{0};    // 존 입장 완료
    std::atomic<int> closed{0};    // 끊긴 봇 (누적)

    std::atomic<int> loginFail{0};
    std::atomic<int> selectFail{0};
    std::atomic<int> createFail{0};

    std::atomic<uint64_t> recvPackets{0};
    std::atomic<uint64_t> recvBytes{0};
    std::atomic<uint64_t> sentPackets{0};
    std::atomic<uint64_t> sentBytes{0};

    std::atomic<uint64_t> deaths{0};          // 봇 사망
    std::atomic<uint64_t> moveCorrections{0}; // 서버가 위치를 되돌림 (이동 검증 실패)
    std::atomic<uint64_t> skillFails{0};      // S_SKILL_FAIL 수신

    static BotStats& Get()
    {
        static BotStats s;
        return s;
    }
};