#pragma once

#include <mutex>
#include <vector>
#include <memory>    // unique_ptr를 위해 추가
#include <algorithm> // std::remove를 위해 추가

#include "..\AStar.h" // Pos 구조체가 정의되어 있다고 가정

class Session;

struct Sector
{
    // mutex는 복사/이동이 불가능하므로 Sector 자체도 복사/이동 불가
    std::mutex sectorMutex;
    std::vector<Session*> sessions;

    void Add(Session* session) {
        std::lock_guard<std::mutex> lock(sectorMutex);
        sessions.push_back(session);
    }

    void Remove(Session* session) {
        std::lock_guard<std::mutex> lock(sectorMutex);
        // std::remove를 위해 <algorithm> 필요
        sessions.erase(std::remove(sessions.begin(), sessions.end(), session), sessions.end());
    }
};

class SectorManager
{
private:
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