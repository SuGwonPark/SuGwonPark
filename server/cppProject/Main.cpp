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