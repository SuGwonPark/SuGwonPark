#pragma once


class Player {
private:
	int id_;
	std::string name_;
	float x_;
	float y_;
	int hp_;
	int exp_;

public:
	Player(int id, std::string name);

	void Move(float x, float y);
	void TakeDamage(int damage);
	bool IsAlive() const;
	void PrintStatus() const;
	std::string GetPlayerName() const;

	uint64_t GetExp() const { return exp_; }
	void AddExp(uint64_t exp) { exp_ += exp; }

	int GetLevel() const;

};