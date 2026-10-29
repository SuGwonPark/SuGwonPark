#include "pch.h"
#include "Network/GatewaySession.h"
#include "Game/Zone.h"
#include "ServerCore/DB/GameDB.h"
#include "ServerCore/Utils/Config.h"
#include "Template/ZoneTemplate.h"
#include "Template/Template.h"
#include "Game/ZoneConstants.h"

int main(int argc, char* argv[])
{
    SetConsoleOutputCP(CP_UTF8); // ← 추가: 콘솔 출력을 UTF-8로
    SetConsoleCP(CP_UTF8);       // ← 추가: 콘솔 입력도 UTF-8로

    // 실행 환경 설정 (Config/server.ini)
    const ServerConfig cfg = LoadServerConfig();

    // DataTemplate 불러오기
    if (!Template::LoadTemplate())
    {
        std::cerr << "[Template] 로드 실패, 서버를 종료합니다." << std::endl;
        return 3;
    }

    // 추후 config로 처리
    std::map<uint32_t, ZoneData> zoneTemplate = ZoneTemplate::Instance().All();

    try
    {
        //	// GameDB 연결
        GameDB& gameDB = GameDB::Instance();
        gameDB.Init(4, "mysqlx://root:qkrtnrnjs1234%21%40%23%24@localhost:33060/game");
    }
    catch (const mysqlx::Error& err)
    {
        std::cerr << "MySQL Error: " << err.what() << std::endl;
        return 1;
    }
    catch (const std::exception& err)
    {
        // 기타 표준 예외 감지
        std::cerr << "Standard Exception: " << err.what() << std::endl;
        return 2;
    }
    boost::asio::io_context io;

    // 틱 / 자동 저장 등에서 쓰려고 Zone 목록을 들고 있는다
    std::vector<ZoneRef> zones;

    for (auto& [zoneID, zone] : zoneTemplate)
    {
        for (uint32_t channelID = 1; channelID <= zone.channelCount; ++channelID)
        {
            auto z = std::make_shared<Zone>(zoneID, channelID);
            // 미리 몬스터 생성
            z->InitMonster();
            zones.push_back(z);
            // 존 등록
            GatewaySession::GetInstance()->AddZone(z);
        }
    }

    // Gateway로 접속 (Gateway의 InternalServer가 듣고 있는 포트로)
    GatewaySession::GetInstance()->Connect(io, cfg.gatewayHost, cfg.internalPort);

    // 존 틱 타이머: ZONE_TICK_MS마다 모든 Zone에 틱 요청
    boost::asio::steady_timer tickTimer(io);
    auto nextTick = std::chrono::steady_clock::now();
    std::function<void()> scheduleTick;
    scheduleTick = [&]() {
        // 타이머 설정
        nextTick += std::chrono::milliseconds(ZoneConst::ZONE_TICK_MS);
        tickTimer.expires_at(nextTick);
        // 스케쥴 실행
        tickTimer.async_wait([&](const boost::system::error_code& ec) {
            if (ec)
                return; // cancel() 되면 멈춤 (종료 처리용)
            for (auto& z : zones)
                z->RequestTick();
            scheduleTick();
        });
    };

    scheduleTick();

    // 종료 신호 (Ctrl+C / SIGTERM): 전원 저장 → DB 큐 비우기 → 종료
    boost::asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code& ec, int sig) {
        if (ec)
            return;
        std::cout << "[Shutdown] 종료 신호(" << sig << ") 수신, 저장 중..." << std::endl;

        tickTimer.cancel(); // 더 이상 틱 안 보냄

        auto finish = [&io]() {
            boost::asio::post(io, [&io]() {    // io 스레드에서 마무리
                GameDB::Instance().Shutdown(); // 남은 저장 전부 실행 후 DB 스레드 종료
                std::cout << "[Shutdown] 저장 완료, 종료합니다." << std::endl;
                io.stop();
            });
        };

        if (zones.empty())
        {
            finish();
            return;
        }

        // 모든 Zone이 저장 요청을 끝내면 마지막 하나가 finish 호출
        auto remaining = std::make_shared<std::atomic<size_t>>(zones.size());
        for (auto& z : zones)
        {
            z->Shutdown([remaining, finish]() {
                if (remaining->fetch_sub(1) == 1)
                    finish();
            });
        }
    });

	while (true)
    {
        try
        {
            io.run();
            break;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Main] io 핸들러 예외: " << e.what() << std::endl;
        }
    } 

    return 0;
}