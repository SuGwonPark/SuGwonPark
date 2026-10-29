#include "Bot.h"
#include "BotStats.h"

#include <atomic>
#include <iomanip>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

// 사용법: BotClient.exe <봇 수> [비밀번호] [시작 번호] [배회 범위] [host] [port]
//   예) BotClient.exe 200 1234 1 100   → bot1 ~ bot200, (-100~100) 범위에서 배회
int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    BotConfig cfg;
    int count = 1000;
    int startIndex = 1;
    try
    {
        if (argc > 1)
            count = std::stoi(argv[1]);
        if (argc > 2)
            cfg.password = argv[2];
        if (argc > 3)
            startIndex = std::stoi(argv[3]);
        if (argc > 4)
            cfg.areaHalf = std::stof(argv[4]);
        if (argc > 5)
            cfg.host = argv[5];
        if (argc > 6)
            cfg.port = static_cast<uint16_t>(std::stoi(argv[6]));
    }
    catch (const std::exception&)
    {
        std::cerr << "사용법: BotClient.exe <봇 수> [비밀번호] [시작 번호] [배회 범위] [host] [port]" << std::endl;
        return 1;
    }

    std::cout << "봇 " << count << "개 (" << cfg.idPrefix << startIndex << " ~ " << cfg.idPrefix
              << (startIndex + count - 1) << ") → " << cfg.host << ":" << cfg.port << std::endl;
    std::cout << "Enter 키를 누르면 종료합니다." << std::endl;

    net::io_context io;
    auto work = net::make_work_guard(io);

    const unsigned threadCount = std::max(2u, std::thread::hardware_concurrency());
    std::vector<std::thread> threads;
    for (unsigned i = 0; i < threadCount; ++i)
        threads.emplace_back([&io]() { io.run(); });

    // 5초마다 통계 출력
    std::atomic<bool> running{true};
    std::thread reporter([&running]() {
        constexpr int INTERVAL_SEC = 5;
        BotStats& s = BotStats::Get();
        uint64_t lastRecvP = 0, lastRecvB = 0, lastSentP = 0;
        while (running)
        {
            for (int i = 0; i < INTERVAL_SEC * 10 && running; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

            const uint64_t rp = s.recvPackets, rb = s.recvBytes, sp = s.sentPackets;
            std::cout << std::fixed << std::setprecision(1) << "[통계] 접속 " << s.connected << " / 게임중 " << s.inGame
                      << " / 끊김 " << s.closed << " | 수신 " << (rp - lastRecvP) / INTERVAL_SEC << " pkt/s, "
                      << (rb - lastRecvB) / 1024.0 / INTERVAL_SEC << " KB/s | 송신 " << (sp - lastSentP) / INTERVAL_SEC
                      << " pkt/s | 사망 " << s.deaths << ", 위치보정 " << s.moveCorrections << ", 스킬실패 "
                      << s.skillFails << " | 실패(로그인/생성/선택) " << s.loginFail << "/" << s.createFail << "/"
                      << s.selectFail << std::endl;
            lastRecvP = rp;
            lastRecvB = rb;
            lastSentP = sp;
        }
    });

    // 한꺼번에 몰지 않고 초당 20개씩 접속 (로그인 비밀번호 검증이 무거움)
    constexpr int CONNECT_PER_SEC = 20;
    std::vector<BotRef> bots;
    bots.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        auto bot = std::make_shared<Bot>(io, cfg, startIndex + i);
        bot->Start();
        bots.push_back(bot);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / CONNECT_PER_SEC));
    }

    std::cin.get();

    // 종료: 봇 정리 → 잠깐 대기(소켓 닫힘 처리) → io 정지
    running = false;
    for (auto& bot : bots)
        bot->Stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    work.reset();
    io.stop();
    for (auto& t : threads)
        t.join();
    reporter.join();
    return 0;
}