#include "pch.h"
#include "Network/Server.h"
#include "Manager/RoomManager.h"
#include "Manager/PlayerManager.h"
#include "DB/DBConnection.h"


int main() {
	DBConnection db;

	// Mysql 연결
	if (!db.Connect("127.0.0.1", "root", "gpdlgh1234!@#$", "gamedb", 33060)) {
		return -1;
	}

	// 서버 시작
	boost::asio::io_context io;
	Server server(io, 8080);
	std::cout << "\n서버 시작 - 포트 8080" << std::endl;
	io.run();

	return 0;
}