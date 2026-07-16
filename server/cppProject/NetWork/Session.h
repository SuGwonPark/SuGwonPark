#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <string>

using boost::asio::ip::tcp;

// 세션 상태 (어느 단계에 있는지)
enum class SessionState {
	CONNECTED,   // 접속만 된 상태
	LOGGEDIN,    // 로그인 완료
	INLOBBY,     // 로비 대기 중
	INROOM,      // 게임방 안
};

class Session : public std::enable_shared_from_this<Session> {
public:
	Session(tcp::socket socket);

	void Start();
	void Send(const char* data, std::size_t length);  // 클라이언트로 데이터 전송

	SessionState state_ = SessionState::CONNECTED;
	int playerId_ = -1;   // 로그인 후 설정됨
	int roomId_ = -1;   // 방 입장 후 설정됨

private:
	void ReadData();

	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];
};