#include "pch.h"
#include "Network/Session.h"
#include "Network/PacketHandler.h"


Session::Session(tcp::socket socket)
	: socket_(std::move(socket)) {
}

void Session::Start() {
	std::cout << "클라이언트 접속!" << std::endl;
	ReadData();
}

// 데이터 읽기
void Session::ReadData() {
	auto self = shared_from_this();
	socket_.async_read_some(boost::asio::buffer(data_, max_length),
		[this, self](boost::system::error_code ec, std::size_t length) {
			if (!ec) {
				// 전송 데이터 타입에 따른 처리 (사실상 Request 구간)
				PacketHandler::Handle(shared_from_this(), data_, length);
				ReadData();
			}
			else {
				std::cout << "클라이언트 연결 끊김 (playerId: " << playerId_ << ")" << std::endl;
			}
		});
}

// 데이터 전송
void Session::Send(const char* data, std::size_t length) {
	auto self = shared_from_this();
	boost::asio::async_write(socket_, boost::asio::buffer(data, length),
		[this, self](boost::system::error_code ec, std::size_t) {
			if (ec) {
				std::cout << "전송 실패" << std::endl;
			}
		});
}