#include "pch.h"
#include "Network/GatewaySession.h"
#include "Game/Zone.h"
#include "ServerCore/DB/GameDB.h"
#include "ServerCore/Utils/Config.h"
#include "Template/ZoneTemplate.h"
#include "Template/Template.h"

int main(int argc, char* argv[]) {
	// 실행 환경 설정 (Config/server.ini)
	const ServerConfig cfg = LoadServerConfig();

	// DataTemplate 불러오기
	if (!Template::LoadTemplate()) {
		std::cerr << "[Template] 로드 실패, 서버를 종료합니다." << std::endl;
		return 3;
	}

	// 추후 config로 처리
	std::map<uint32_t, ZoneData> zoneTemplate = ZoneTemplate::Instance().All();

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

	for (auto& [zoneID, zone] : zoneTemplate) {
		for (uint32_t channelID = 1; channelID <= zone.channelCount; ++channelID) {
			// 존 등록
			GatewaySession::GetInstance()->AddZone(std::make_shared<Zone>(zoneID, channelID));
		}
	}

	// Gateway로 접속 (Gateway의 InternalServer가 듣고 있는 포트로)
	GatewaySession::GetInstance()->Connect(io, cfg.gatewayHost, cfg.internalPort);

	io.run();

	return 0;
}