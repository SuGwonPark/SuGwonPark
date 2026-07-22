#include "pch.h"
#include "Network/Server.h"
#include "Manager/RoomManager.h"
#include "Manager/PlayerManager.h"
#include "DB/DBConnection.h"


int main() {
	DBConnection db;

	// 서버 시작 시 기본 방 생성
	/*RoomManager::GetInstance().CreateRoom(1, "초보자의 마을");
	RoomManager::GetInstance().CreateRoom(2, "전투의 광장");
	RoomManager::GetInstance().CreateRoom(3, "보스 레이드");

	RoomManager::GetInstance().PrintAll();*/


	// 연결
	if (!db.Connect("localhost", "root", "gpdlgh1234!@#$", "gameDB", 33060)) {
		return -1;
	}

	// INSERT
	db.Execute("INSERT INTO players (name, hp) VALUES ('홍길동', 100)");

	// SELECT
	auto result = db.Query("SELECT * FROM players");
	for (auto row : result) {
		std::cout << "이름: " << row[0] << std::endl;
		std::cout << "HP: " << row[1] << std::endl;
	}

	// UPDATE
	db.Execute("UPDATE players SET hp = 200 WHERE name = '홍길동'");

	// DELETE
	db.Execute("DELETE FROM players WHERE name = '홍길동'");

	db.Disconnect();

	boost::asio::io_context io;
	Server server(io, 8080);
	std::cout << "\n서버 시작 - 포트 8080" << std::endl;
	io.run();

	return 0;
}