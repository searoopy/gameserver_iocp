#pragma once


#include <atomic>
#include <vector>
#include "Monster.h"

#define MONSTER_START_ID 100000

class MonsterManager
{
public:
	MonsterManager() {}
	~MonsterManager()
	{
		for (Monster* monster : m_monsters)
		{
			delete monster;
		}
		m_monsters.clear();
	}

	void CreateMonsters(int count);

	std::vector<Monster*>& GetMonsters() { return m_monsters; }

private:
	std::vector<Monster*> m_monsters;
};




extern std::unique_ptr < MonsterManager> g_pMonsterManager;