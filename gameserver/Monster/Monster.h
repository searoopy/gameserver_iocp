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
		:monsterId(id), hp(100), state(MONSTER_STATE::IDLE),
		moveTimer(0.0f) , speed(0.5f)
	{
		pos.x = 0; pos.y = 0;
		
	}


public:
	int32_t monsterId;
	std::atomic<int32_t> hp;
	float moveTimer;
	float speed;

	struct {
		std::atomic<int32_t> x;
		std::atomic<int32_t> y;
	} pos;


	MONSTER_STATE state;
	std::mutex m_lock;




};