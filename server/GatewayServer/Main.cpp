#include "pch.h"
#include "Network/Server.h"
#include "ServerCore/DB/GameDB.h"
#include "ServerCore/DB/AccountDB.h"
#include "Network/InternalServer.h"
#include "GatewayConstants.h"
#include "ServerCore/Utils/Config.h"


int main() {
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	// 실행 환경 설정 (Config/server.ini)
	const ServerConfig cfg = LoadServerConfig();

	// libsodium 초기화 (crypto_pwhash_* 사용 전에 반드시 한 번)
	if (sodium_init() < 0) {
		std::cerr << "sodium_init failed" << std::endl;
		return 1;
	}

	try {
		//	// GameDB 연결
		GameDB& gameDB = GameDB::Instance();
		gameDB.Init(4, "mysqlx://root:qkrtnrnjs1234%21%40%23%24@localhost:33060/game");

		AccountDB& accountDB = AccountDB::Instance();
		accountDB.Init(4, "mysqlx://root:qkrtnrnjs1234%21%40%23%24@localhost:33060/account");
	}
	catch (const mysqlx::Error& err) {
		std::cerr << "MySQL Error: " << err.what() << std::endl;
		return 2;
	}
	catch (const std::exception& err) {
		// 기타 표준 예외 감지
		std::cerr << "Standard Exception: " << err.what() << std::endl;
		return 3;
	}

	// 서버 시작
	boost::asio::io_context io;
	Server server(io, cfg.clientPort); // 게임 서버 
	std::cout << "\n서버 시작 - 포트 " << cfg.clientPort << std::endl;

	InternalServer internalServer(io, cfg.internalPort); // zone서버
	std::cout << "\n서버 시작 - 포트 " << cfg.internalPort << std::endl;

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