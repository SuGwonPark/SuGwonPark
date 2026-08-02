#pragma once


class Monster
{
private:
	std::string name_;
	int hp_;

public:
	// 초기화 리스트 방식
	Monster(std::string name, int hp);

	void TakeDamage(int damage);

	void PrintStatus();
};