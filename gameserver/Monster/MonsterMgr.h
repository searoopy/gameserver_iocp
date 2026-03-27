#pragma once


#include <atomic>
#include <vector>
#include "Monster.h"

//#include "..\GROUND_TILE\AStar.h"
//#include "..\Common.h"


#define MONSTER_START_ID 100000

struct Session;
struct OverlappedEx;
struct Pos;

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

	void UpdateMonsters(float deltaTime);
	//void UpdateMonsterSector(Monster* m, int oldx, int oldy, int newX, int newY);
	void ProcessMonsterChase(Monster* monster, Session* closestUser);
	void SyncMonsterView(Monster* monster, const Pos& oldIdx, const  Pos& newIdx);
	void ProcessMonsterMove(Monster* m, int nextX, int nextY);



	//통신 프로토콜.
	void BroadcastMonsterMoveToNearby(Monster* m);
	void BroadcastToList(const std::vector<Session*>& targets, OverlappedEx* pkt);

private:
	std::vector<Monster*> m_monsters;
};


extern std::unique_ptr < MonsterManager> g_pMonsterManager;