#pragma once


class Player {
private:
	int id_;
	std::string name_;
	float x_;
	float y_;
	int hp_;

public:
	Player(int id, std::string name);

	void Move(float x, float y);
	void TakeDamage(int damage);
	bool IsAlive() const;
	void PrintStatus() const;
	std::string GetPlayerName() const;
};