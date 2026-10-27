#include "pch.h"
#include "Network/Server.h"
#include "Network/ClientProxySession.h"
#include "Manager/SessionManager.h"

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
				boost::system::error_code epEc;
				const auto remote = session->Socket().remote_endpoint(epEc);   // 예외 대신 ec로 받음
				if (!epEc) {
					std::cout << "클라이언트 접속: " << remote
						<< " (sessionId=" << session->GetSessionID() << ")" << std::endl;
					SessionManager::GetInstance()->Register(session->GetSessionID(), session);
					session->Start();
				}
				// epEc면 접속 직후 끊긴 것: 이 세션은 버리고 다음 접속을 기다린다
			}
			else if (ec == boost::asio::error::operation_aborted) {
				return;          // acceptor가 닫힘 (종료 중): 다시 accept하지 않음
			}
			Accept();
		
		});
}
