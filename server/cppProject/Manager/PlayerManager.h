#pragma once
#include "Player.h"
#include <map>
#include <memory>
#include <mutex>

class PlayerManager {
public:
	static PlayerManager& GetInstance();  // ΩÃ±€≈Ê

	void AddPlayer(int id, std::string name);
	void RemovePlayer(int id);
	std::shared_ptr<Player> GetPlayer(int id);
	void PrintAll() const;

private:
	PlayerManager() {}  // ΩÃ±€≈Ê¿Ã∂Û ø‹∫Œø°º≠ ª˝º∫ ∫“∞°

	std::map<int, std::shared_ptr<Player>> players_;
	mutable std::mutex mtx_;
};