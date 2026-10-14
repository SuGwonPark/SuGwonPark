#include "pch.h"
#include "Network/Server.h"
#include "DB/GameDB.h"
#include "DB/AccountDB.h"
#include "Network/InternalServer.h"


int main() {
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	// libsodium 초기화 (crypto_pwhash_* 사용 전에 반드시 한 번)
	if (sodium_init() < 0) {
		std::cerr << "sodium_init failed" << std::endl;
		return 1;
	}

	try {
		//	// GameDB 연결
		GameDB& gameDB = GameDB::Instance();
		gameDB.Init(4, "mysqlx://root:gpdlgh1234%21%40%23%24@localhost:33060/game");

		AccountDB& accountDB = AccountDB::Instance();
		accountDB.Init(4, "mysqlx://root:gpdlgh1234%21%40%23%24@localhost:33060/account");
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
	Server server(io, 8080); // 게임 서버 
	std::cout << "\n서버 시작 - 포트 8080" << std::endl;

	InternalServer internalServer(io, 9000); // zone서버
	std::cout << "\n서버 시작 - 포트 9000" << std::endl;
	io.run();

	return 0;
}