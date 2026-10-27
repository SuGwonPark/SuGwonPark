#include "pch.h"
#include "Network/InternalServer.h"
#include "Network/ZoneServerSession.h"
#include "Manager/ZoneManager.h"

InternalServer::InternalServer(net::io_context& io, short port)
	: io_(io)
	Accept();
}

void InternalServer::Accept() {
	auto session = std::make_shared<ZoneServerSession>(io_); // zoneId는 접속 직후 핸드셰이크로 받아야 함 (임시 0)

	acceptor_.async_accept(session->Socket(),
		[this, session](boost::system::error_code ec) {
			if (!ec) {
                boost::system::error_code epEc;
                const auto remote = session->Socket().remote_endpoint(epEc); // 예외 대신 ec로 받음
                if (!epEc)
                {
                    std::cout << "Zone 서버 접속: " << remote << std::endl;
                    session->Start(); // 등록은 핸드셰이크 받은 뒤 ZoneManager에서
                }
            }
            else if (ec == boost::asio::error::operation_aborted)
            {
                return;
            }
			Accept();
		});
}
	, acceptor_(io, tcp::endpoint(net::ip::make_address("127.0.0.1"), port)) {