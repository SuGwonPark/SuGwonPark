#pragma once
#include "Game/Player.h"


class PlayerManager {
public:
	static PlayerManager& GetInstance();  // ½Ì±ÛÅæ

	void AddPlayer(int id, std::string name);
	void RemovePlayer(int id);
	std::shared_ptr<Player> GetPlayer(int id);
	void PrintAll() const;

private:
	PlayerManager() {}  // ½Ì±ÛÅæÀÌ¶ó ¿ÜºÎ¿¡¼­ »ı¼º ºÒ°¡

	std::map<int, std::shared_ptr<Player>> players_;
	mutable std::mutex mtx_;
};