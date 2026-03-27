#pragma once

#include <mutex>
#include <vector>
#include <memory>    // unique_ptr를 위해 추가
#include <algorithm> // std::remove를 위해 추가
#include <shared_mutex>

#include "..\AStar.h" // Pos 구조체가 정의되어 있다고 가정



class Monster;
struct Session;

struct Sector
{
    // mutex는 복사/이동이 불가능하므로 Sector 자체도 복사/이동 불가
   
    //유저는 읽기가 더 빈번하므로 공유뮤텍스 사용해보자
    mutable std::shared_mutex sessionLock;
    std::vector<Session*> sessions;

    std::mutex monsterLock;
    std::vector<Monster*> monsters;

    void Add(Session* session) {
        //std::lock_guard<std::mutex> lock(sectorMutex);
        std::unique_lock<std::shared_mutex> lock(sessionLock); // 쓰기 모드 (독점)
        sessions.push_back(session);
    }

    void Remove(Session* session) {
       // std::lock_guard<std::mutex> lock(sectorMutex);
        // std::remove를 위해 <algorithm> 필요
        std::unique_lock<std::shared_mutex> lock(sessionLock); // 쓰기 모드 (독점)
        sessions.erase(std::remove(sessions.begin(), sessions.end(), session), sessions.end());
    }


    void AddMonster(Monster* m) {
        std::lock_guard<std::mutex> lock(monsterLock);
        monsters.push_back(m);
    }

    void RemoveMonster(Monster* m) {
        std::lock_guard<std::mutex> lock(monsterLock);
       // monsters.remove(m);
        monsters.erase(std::remove(monsters.begin(), monsters.end(), m), monsters.end());
    }

};

class SectorManager
{
private:
    /*
    섹터 사이즈에 대하여....

    width, height는 tilemgr의 타일 갯수이다
    섹터사이즈는 이 타일의 개수라고 할수있다

    10일경우 상하좌우 10칸의 공간이 하나의 섹터..
    
    
    */
    const int SECTOR_SIZE = 10;
    int m_width;
    int m_height;
    int m_sectorCountX;
    int m_sectorCountY;

    // 핵심 수정: Sector를 직접 넣지 않고 포인터로 관리하여 mutex 복사 문제 해결
    std::vector<std::unique_ptr<Sector>> m_sectors;

public:
    void Init(int mapWidth, int mapHeight);


    bool IsValidSector(int x, int y);

    Pos GetSectorIndex(int x, int y);

    Sector& GetSector(int sX, int sY);
    Sector& GetSector(Pos pos);

    Sector& GetSectorByTile(int tileX, int tileY);

    void GetNearbySessions(int x, int y, std::vector<Session*>& outSessions);

   
};

extern std::unique_ptr <SectorManager> g_pSectorMgr;