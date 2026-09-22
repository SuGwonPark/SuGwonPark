#pragma once
#include "Game/Player.h"


class PlayerManager {
public:
	static PlayerManager& GetInstance();  // 싱글톤

	void AddPlayer(int id, std::string name);
	void RemovePlayer(int id);
	std::shared_ptr<Player> GetPlayer(int id);
	void PrintAll() const;

private:
	PlayerManager() {}  // 싱글톤이라 외부에서 생성 불가

	std::map<int, std::shared_ptr<Player>> players_;
	mutable std::mutex mtx_;
};