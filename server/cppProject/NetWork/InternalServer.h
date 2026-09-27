#pragma once

// Zone 서버들이 접속해오는 내부 전용 accept 루프
class InternalServer {
public:
	InternalServer(net::io_context& io, short port);

private:
	void Accept();

private:
	net::io_context& io_;
	tcp::acceptor acceptor_;
};