#include "pch.h"
#include "Network/Server.h"
#include "Manager/RoomManager.h"
#include "Manager/PlayerManager.h"
#include "DB/GameDB.h"
#include "DB/GameDBShard.h"


int main() {
	// GameDB 연결
	GameDB& gameDB = GameDB::Instance();
	gameDB.Init(4, "mysqlx://root:gpdlgh1234!@#$@localhost:33060/gameDB");

	GameDBShard& gameShardDB = GameDBShard::Instance();
	std::vector<std::string> dbUrls = {
		"mysqlx://root:gpdlgh1234!@#$@localhost:33060/shardDB_0",
		"mysqlx://root:gpdlgh1234!@#$@localhost:33060/shardDB_1",
		"mysqlx://root:gpdlgh1234!@#$@localhost:33060/shardDB_2",
		"mysqlx://root:gpdlgh1234!@#$@localhost:33060/shardDB_3"
	};
	gameShardDB.Init(2, dbUrls);


	// 서버 시작
	boost::asio::io_context io;
	Server server(io, 8080);
	std::cout << "\n서버 시작 - 포트 8080" << std::endl;
	io.run();

	return 0;
}