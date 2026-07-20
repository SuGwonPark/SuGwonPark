#pragma once


class Session;

class Room {
public:
	Room(int id, std::string name);

	bool Join(std::shared_ptr<Session> session);
	void Leave(int playerId);
	void Broadcast(const char* data, std::size_t length, int excludePlayerId = -1);
	bool IsFull() const;
	void PrintStatus() const;

	int id_;
	std::string name_;

private:
	std::map<int, std::shared_ptr<Session>> sessions_;
	mutable std::mutex mtx_;
	const int maxPlayers_ = 10;
};