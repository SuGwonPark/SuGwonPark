#pragma once

class ClientProxySession;

// 게이트웨이 accept 루프. 접속이 들어오면 ClientProxySession을 만들어 Zone으로 릴레이한다.
class Server {
public:
	Server(boost::asio::io_context& io, short port);

private:
	void Accept();

private:
	net::io_context& io_;
	tcp::acceptor acceptor_;
	std::atomic<uint64_t> nextSessionId_{ 1 };
};
