#include "pch.h"
#include "Network/GatewaySession.h"
#include "Game/Zone.h"
#include "ServerCore/DB/GameDB.h"
#include "ServerCore/Utils/Config.h"


int main(int argc, char* argv[]) {
	// 실행 환경 설정 (Config/server.ini)
	const ServerConfig cfg = LoadServerConfig();

	// 추후 config로 처리
	std::vector<std::pair<uint32_t, uint32_t>> zoneConfig = {
		{1,1}, {1,2}, {1,3}, {1,4},
		{2,1}, {2,2}, {2,3},
		{3,1}, {3,2},
	};

	try {
		//	// GameDB 연결
		GameDB& gameDB = GameDB::Instance();
		gameDB.Init(4, "mysqlx://root:gpdlgh1234%21%40%23%24@localhost:33060/game");

	}
	catch (const mysqlx::Error& err) {
		std::cerr << "MySQL Error: " << err.what() << std::endl;
		return 1;
	}
	catch (const std::exception& err) {
		// 기타 표준 예외 감지
		std::cerr << "Standard Exception: " << err.what() << std::endl;
		return 2;
	}
	boost::asio::io_context io;

	for (auto& [key, value] : zoneConfig)
	{
		// 존 등록
		GatewaySession::GetInstance()->AddZone(std::make_shared<Zone>(key, value));
	}

	// Gateway로 접속 (Gateway의 InternalServer가 듣고 있는 포트로)
	GatewaySession::GetInstance()->Connect(io, cfg.gatewayHost, cfg.internalPort);

	io.run();

	return 0;
}