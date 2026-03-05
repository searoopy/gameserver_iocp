#pragma once

#include <atomic>
#include <mutex>

enum class MONSTER_STATE 
{
	IDLE, 
	CHASE, 
	ATTACK, 
	DEAD
};


class Monster
{
public:
	Monster(int32_t id)
		:monsterId(id), hp(100), state(MONSTER_STATE::IDLE)
	{
		pos.x = 0; pos.y = 0;
	}


public:
	int32_t monsterId;
	std::atomic<int32_t> hp;

	struct {
		std::atomic<float> x;
		std::atomic<float> y;
	} pos;


	MONSTER_STATE state;
	std::mutex m_lock;

};