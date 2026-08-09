#include "pch.h"
#include "Room.h"
#include "Network/Session.h"

Room::Room(int id, std::string name)
	: id_(id), name_(name) {
}

bool Room::Join(std::shared_ptr<Session> session) {
	std::lock_guard<std::mutex> lock(mtx_);
	if (sessions_.size() >= maxPlayers_) return false;

	sessions_[session->getPlayerID()] = session;
	std::cout << "플레이어(" << session->getPlayerID() << ") → 방[" << name_ << "] 입장" << std::endl;
	return true;
}

void Room::Leave(int playerId) {
	std::lock_guard<std::mutex> lock(mtx_);
	sessions_.erase(playerId);
	std::cout << "플레이어(" << playerId << ") → 방[" << name_ << "] 퇴장" << std::endl;
}

void Room::Broadcast(const char* data, std::size_t length, int excludePlayerId) {
	std::lock_guard<std::mutex> lock(mtx_);
	for (auto& pair : sessions_) {
		if (pair.first != excludePlayerId) {
			pair.second->Send(data, length);  // 방 안 모든 플레이어에게 전송
		}
	}
}

bool Room::IsFull() const {
	std::lock_guard<std::mutex> lock(mtx_);
	return sessions_.size() >= maxPlayers_;
}

void Room::PrintStatus() const {
	std::lock_guard<std::mutex> lock(mtx_);
	std::cout << "방[" << name_ << "] 인원: "
		<< sessions_.size() << "/" << maxPlayers_ << std::endl;
}