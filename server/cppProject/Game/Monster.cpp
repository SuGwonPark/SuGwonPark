#include "pch.h"
#include "Monster.h"

// 초기화 리스트 방식
Monster::Monster(std::string name, int hp) : name_(name), hp_(hp) {}

void Monster::TakeDamage(int damage) {
	hp_ -= damage;
}

void Monster::PrintStatus() {
	std::cout << name_ << "의 현재 HP: " << hp_ << std::endl;

}

