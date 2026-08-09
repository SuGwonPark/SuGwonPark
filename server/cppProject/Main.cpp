#include "pch.h"
#include "Network/Server.h"
#include "Manager/RoomManager.h"
#include "Manager/PlayerManager.h"
#include "DB/GameDB.h"
#include "DB/GameDBShard.h"


int main() {
	try {
		//	// GameDB 연결
		GameDB& gameDB = GameDB::Instance();
		gameDB.Init(4, "mysqlx://root:gpdlgh1234%21%40%23%24@localhost:33060/gamedb");

		GameDBShard& gameShardDB = GameDBShard::Instance();
		std::vector<std::string> dbUrls = {
			"mysqlx://root:gpdlgh1234%21%40%23%24@localhost:33060/game_shard_01",
			"mysqlx://root:gpdlgh1234%21%40%23%24@localhost:33060/game_shard_02",
			"mysqlx://root:gpdlgh1234%21%40%23%24@localhost:33060/game_shard_03",
			"mysqlx://root:gpdlgh1234%21%40%23%24@localhost:33060/game_shard_04"
		};
		gameShardDB.Init(4, dbUrls);
	}
	catch (const mysqlx::Error& err) {

		std::cerr << "MySQL Error: " << err.what() << std::endl;
	}
	catch (const std::exception& err) {
		// 기타 표준 예외 감지
		std::cerr << "Standard Exception: " << err.what() << std::endl;
	}

	// 서버 시작
	boost::asio::io_context io;
	Server server(io, 8080);
	std::cout << "\n서버 시작 - 포트 8080" << std::endl;
	io.run();

	return 0;
}