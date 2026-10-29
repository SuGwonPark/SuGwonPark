#pragma once
#include <boost/asio.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "ServerCore/Protocol/Packets.h"

namespace net = boost::asio;
using tcp = net::ip::tcp;

struct BotConfig
{
    std::string host = "127.0.0.1";
    uint16_t port = 8080;
    std::string idPrefix = "bot"; // 계정 id = bot1, bot2 ... (캐릭터 이름도 같은 값)
    std::string password = "testpassword";
    float areaHalf = 100.f; // 배회 범위: x, z 모두 -areaHalf ~ +areaHalf
    float moveSpeed = 5.f;  // 초당 이동 거리 (서버 MAX_MOVE_SPEED 10 이하)
    int tickMs = 200;       // 행동 주기
};

// 봇 1개 = 클라이언트 1개. 모든 핸들러는 이 봇의 strand에서만 실행된다 (멤버에 락 불필요)
class Bot : public std::enable_shared_from_this<Bot>
{
  public:
    using Clock = std::chrono::steady_clock;

    Bot(net::io_context& io, const BotConfig& cfg, int index);

    void Start(); // 접속 → 로그인 → 캐릭터 선택/생성 → 입장 → 행동 루프
    void Stop();  // 다른 스레드에서 불러도 됨

  private:
    enum class State : uint8_t
    {
        Connecting,
        LoggingIn,
        Lobby,
        Selecting,
        Spawning,
        InGame,
        Closed,
    };

    struct MonsterView
    {
        float x = 0.f;
        float z = 0.f;
        int32_t hp = 0;
    };

    // 네트워크
    void DoRead();
    void ProcessPackets();
    void Send(const void* data, uint16_t size);
    void DoWrite();
    void Close(const char* reason);

    template <typename T>
    void SendPacket(T& pkt, PacketID id)
    {
        pkt.header.id = id;
        pkt.header.size = sizeof(T);
        Send(&pkt, sizeof(T));
    }

    // 패킷 처리
    void HandlePacket(uint16_t id, const uint8_t* data, uint16_t size);
    void SelectCharacter(uint64_t playerID);
    void CreateCharacter();

    // 행동
    void ScheduleTick();
    void OnTick();
    void MoveToward(float tx, float tz, float step, float stopDist);
    void PickWaypoint();

  private:
    const BotConfig cfg_;
    const int index_;
    std::string accountID_;

    net::strand<net::io_context::executor_type> strand_;
    tcp::socket socket_;
    net::steady_timer tick_;
    State state_ = State::Connecting;
    bool countedConnected_ = false;

    // 수신 버퍼 (서버 패킷 최대 65535 → 한 개는 항상 들어감)
    std::array<uint8_t, 65536> recvBuf_{};
    size_t readPos_ = 0;
    size_t writePos_ = 0;

    // 송신 큐 (deque는 push_back해도 기존 원소 주소가 안 바뀜 → 전송 중 버퍼 안전)
    std::deque<std::vector<uint8_t>> sendQueue_;

    // 게임 상태 (서버가 알려준 값)
    uint64_t myID_ = 0;
    float x_ = 0.f, y_ = 0.f, z_ = 0.f;
    int32_t hp_ = 0;
    int32_t mp_ = 0;
    std::unordered_map<uint64_t, MonsterView> monsters_; // 시야 안 몬스터

    // 행동 상태
    float wx_ = 0.f, wz_ = 0.f; // 배회 목표 지점
    Clock::time_point nextAttackAt_{};
    Clock::time_point nextSkillAt_{};
    Clock::time_point nextReviveAt_{};
    std::mt19937 rng_;
};

using BotRef = std::shared_ptr<Bot>;