#include "pch.h"
#include "Network/GatewaySession.h"
#include "Game/Zone.h"
#include "DB/GameDB.h"


int main(int argc, char* argv[]) {
	if (argc < 3) {
		std::cerr << "사용법: ZoneServer.exe <zoneID>" << std::endl;
		return -1;
	}
	uint32_t zoneID = static_cast<uint32_t>(std::stoul(argv[1]));
	uint32_t channelID = static_cast<uint32_t>(std::stoul(argv[2]));

	// 추후 config로 처리
	std::map<uint32_t, uint32_t> zoneConfig;
	// 임시
	zoneConfig[1] = 1;
	zoneConfig[1] = 2;
	zoneConfig[1] = 3;
	zoneConfig[1] = 4;
	zoneConfig[2] = 1;
	zoneConfig[2] = 2;
	zoneConfig[2] = 3;
	zoneConfig[3] = 1;
	zoneConfig[3] = 2;


	try {
		//	// GameDB 연결
		GameDB& gameDB = GameDB::Instance();
		gameDB.Init(4, "mysqlx://root:gpdlgh1234%21%40%23%24@localhost:33060/game");

	}
	catch (const mysqlx::Error& err) {

		std::cerr << "MySQL Error: " << err.what() << std::endl;
	}
	catch (const std::exception& err) {
		// 기타 표준 예외 감지
		std::cerr << "Standard Exception: " << err.what() << std::endl;

	}
	boost::asio::io_context io;

	for (auto& [key, value] : zoneConfig)
	{
		// 존 등록
		GatewaySession::GetInstance()->AddZone(std::make_shared<Zone>(key, value));
	}

	// Gateway로 접속 (Gateway의 InternalServer가 듣고 있는 포트로)
	GatewaySession::GetInstance()->Connect(io, "127.0.0.1", 9000);

	std::cout << "Zone 서버 시작 - zoneID=" << zoneID << std::endl;
	io.run();

	return 0;
}