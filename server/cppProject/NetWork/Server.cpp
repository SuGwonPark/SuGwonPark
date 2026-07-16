#include "Server.h"
#include "Session.h"
#include <iostream>

using boost::asio::ip::tcp;

Server::Server(boost::asio::io_context& io, short port) : acceptor_(io, tcp::endpoint(tcp::v4(), port)) { Accept(); }

// 유저 접속 대기 상태
void Server::Accept() {
	acceptor_.async_accept(
		[this](boost::system::error_code ec, tcp::socket socket) {
			if (!ec) {
				std::cout << "클라이언트 연결됨: " << socket.remote_endpoint() << std::endl;
				std::make_shared<Session>(std::move(socket))->Start();
			}
			Accept(); // 다음 연결 대기
		});
}
