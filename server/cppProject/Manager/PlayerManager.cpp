#include "pch.h"
#include "PlayerManager.h"


PlayerManager& PlayerManager::GetInstance() {
	static PlayerManager instance;  // 최초 호출 시 한 번만 생성됨
	return instance;
}

void PlayerManager::AddPlayer(int id, std::string name) {
	std::lock_guard<std::mutex> lock(mtx_);
	players_[id] = std::make_shared<Player>(id, name);
	std::cout << name << " 접속! (ID: " << id << ")" << std::endl;
}

void PlayerManager::RemovePlayer(int id) {
	std::lock_guard<std::mutex> lock(mtx_);
	auto it = players_.find(id);
	if (it != players_.end()) {
		std::cout << it->second->GetPlayerName() << " 퇴장!" << std::endl;
		players_.erase(it);
	}
}

std::shared_ptr<Player> PlayerManager::GetPlayer(int id) {
	std::lock_guard<std::mutex> lock(mtx_);
	auto it = players_.find(id);
	if (it == players_.end()) return nullptr;
	return it->second;
}

void PlayerManager::PrintAll() const {
	std::lock_guard<std::mutex> lock(mtx_);
	for (auto& pair : players_) {
		pair.second->PrintStatus();
	}
}