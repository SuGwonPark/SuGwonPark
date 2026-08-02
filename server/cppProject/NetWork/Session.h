#pragma once
#include "Game/Player.h"

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

	uint64_t getPlayerID() const { return playerId_; }
	void setPlayerID(uint64_t id) { playerId_ = id; }

	int getRoomID() const { return roomId_; }
	void setRoomID(int id) { roomId_ = id; }

	std::shared_ptr<Player> GetPlayer() const { return player_; }

	void SavePlayerAsnyc();  // 비동기적으로 플레이어 정보 저장

private:
	void ReadData();

	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];

	uint64_t playerId_ = -1;
	int roomId_ = -1;

	std::shared_ptr<Player> player_;
};