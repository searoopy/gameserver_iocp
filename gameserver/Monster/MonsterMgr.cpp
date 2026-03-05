#include "MonsterMgr.h"

#include "..\Session\SessionManager.h"

MonsterManager g_MonsterManager;


void MonsterManager::CreateMonsters(int count)
{
	for (int i = 0; i < count; ++i)
	{
		int32_t mId = MONSTER_START_ID + i;
		Monster* monster = new Monster(mId);



		//램덤 위치 배치
		monster->pos.x = static_cast<float>(rand() % 375);
		monster->pos.y = static_cast<float>(rand() % 667);

		m_monsters.push_back(monster);

		// 여기서 주변 유저가 있다면 이동 패킷 브로드캐스트!
		GSessionManager.BroadcastMonsterMove(monster);
	}
}