#include "pch.h"
#include "Network/GatewaySession.h"
#include "Game/Zone.h"


int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cerr << "사용법: ZoneServer.exe <zoneId>" << std::endl;
		return -1;
	}
	uint32_t zoneId = static_cast<uint32_t>(std::stoul(argv[1]));

	boost::asio::io_context io;

	// Gateway로 접속 (Gateway의 InternalServer가 듣고 있는 포트로)
	GatewaySession::GetInstance()->Connect(io, "127.0.0.1", 9000, zoneId);

	std::cout << "Zone 서버 시작 - zoneId=" << zoneId << std::endl;
	io.run();

	return 0;
}