#pragma once

class Server {
private:
	void Accept();

	tcp::acceptor acceptor_;
public:
	Server(boost::asio::io_context& io, short port);
};