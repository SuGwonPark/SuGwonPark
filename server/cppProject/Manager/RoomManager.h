// RoomManager.h
#pragma once
#include "Game/Room.h"

class RoomManager {
public:
	static RoomManager& GetInstance();

	void CreateRoom(int id, std::string name);
	std::shared_ptr<Room> GetRoom(int id);
	void PrintAll() const;

private:
	RoomManager() {}
	std::map<int, std::shared_ptr<Room>> rooms_;
	mutable std::mutex mtx_;
};