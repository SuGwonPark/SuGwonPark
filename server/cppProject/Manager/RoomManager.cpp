// RoomManager.cpp
#include "RoomManager.h"
#include <iostream>

RoomManager& RoomManager::GetInstance() {
	static RoomManager instance;
	return instance;
}

void RoomManager::CreateRoom(int id, std::string name) {
	std::lock_guard<std::mutex> lock(mtx_);
	rooms_[id] = std::make_shared<Room>(id, name);
	std::cout << "¹æ »ı¼º: [" << name << "] (ID: " << id << ")" << std::endl;
}

std::shared_ptr<Room> RoomManager::GetRoom(int id) {
	std::lock_guard<std::mutex> lock(mtx_);
	auto it = rooms_.find(id);
	if (it == rooms_.end()) return nullptr;
	return it->second;
}

void RoomManager::PrintAll() const {
	std::lock_guard<std::mutex> lock(mtx_);
	for (auto& pair : rooms_) {
		pair.second->PrintStatus();
	}
}