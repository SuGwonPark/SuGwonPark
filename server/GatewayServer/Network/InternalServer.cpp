#include "pch.h"
#include "Network/InternalServer.h"
#include "Network/ZoneServerSession.h"
#include "Manager/ZoneManager.h"

InternalServer::InternalServer(net::io_context& io, short port)
	: io_(io), acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
	Accept();
}

void InternalServer::Accept() {
	auto session = std::make_shared<ZoneServerSession>(io_); // zoneId는 접속 직후 핸드셰이크로 받아야 함 (임시 0)

	acceptor_.async_accept(session->Socket(),
		[this, session](boost::system::error_code ec) {
			if (!ec) {
				std::cout << "Zone 서버 접속: " << session->Socket().remote_endpoint() << std::endl;
				session->Start();
				// TODO: 접속 직후 Zone이 자기 zoneId를 보내는 핸드셰이크 패킷을 받아서
				// ZoneManager::GetInstance()->RegisterZone(zoneId, session) 호출해야 함
				// (지금은 zoneId를 미리 알 방법이 없어서 핸드셰이크 설계가 필요함)
			}
			Accept();
		});
}