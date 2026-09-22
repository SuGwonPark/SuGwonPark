#include "pch.h"
#include "Network/Server.h"
#include "Network/ClientProxySession.h"

Server::Server(boost::asio::io_context& io, short port)
	: io_(io), acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
	Accept();
}

// 게이트웨이 접속 대기: ClientProxySession을 미리 만들어 그 소켓으로 accept한다
void Server::Accept() {
	auto session = std::make_shared<ClientProxySession>(nextSessionId_++, io_);

	acceptor_.async_accept(session->Socket(),
		[this, session](boost::system::error_code ec) {
			if (!ec) {
				std::cout << "클라이언트 접속: " << session->Socket().remote_endpoint()
					<< " (sessionId=" << session->GetSessionId() << ")" << std::endl;
				session->Start();
			}
			Accept(); // 다음 접속 대기
		});
}
