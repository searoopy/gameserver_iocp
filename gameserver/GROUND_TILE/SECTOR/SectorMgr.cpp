#include "SectorMgr.h"
#include <algorithm>

std::unique_ptr <SectorManager> g_pSectorMgr;

void SectorManager::Init(int mapWidth, int mapHeight)
{
    m_width = mapWidth;
    m_height = mapHeight;

    m_sectorCountX = (mapWidth / SECTOR_SIZE) + 1;
    m_sectorCountY = (mapHeight / SECTOR_SIZE) + 1;

    int totalSectors = m_sectorCountX * m_sectorCountY;

    // reserve + push_back 대신 resize 사용
    m_sectors.clear();
    for (int i = 0; i < totalSectors; ++i) {
        m_sectors.push_back(std::make_unique<Sector>());
    }
}

bool SectorManager::IsValidSector(int x, int y) {
    return (x >= 0 && x < m_sectorCountX && y >= 0 && y < m_sectorCountY);
}

Pos SectorManager::GetSectorIndex(int x, int y) {
    return { x / SECTOR_SIZE, y / SECTOR_SIZE };
}

Sector& SectorManager::GetSector(int sX, int sY) {
    // 인덱스 방어 코드 (std::clamp는 C++17부터 사용 가능)
    int x = std::max(0, std::min(sX, m_sectorCountX - 1));
    int y = std::max(0, std::min(sY, m_sectorCountY - 1));


    // 2. 1차원 인덱스 계산
    int index = y * m_sectorCountX + x;

    // 3. 최종 인덱스 범위 방어 (매우 중요)
    if (index < 0 || index >= (int)m_sectors.size()) {
        // 비정상 상황 시 0번 섹터라도 반환하여 크래시 방지
        return *m_sectors[0];
    }

    return *m_sectors[index];
}

Sector& SectorManager::GetSector(Pos pos) {
    // 인덱스 방어 코드 (std::clamp는 C++17부터 사용 가능)
    int x = std::max(0, std::min(pos.x, m_sectorCountX - 1));
    int y = std::max(0, std::min(pos.y, m_sectorCountY - 1));
    // 2. 1차원 인덱스 계산
    int index = y * m_sectorCountX + x;

    // 3. 최종 인덱스 범위 방어 (매우 중요)
    if (index < 0 || index >= (int)m_sectors.size()) {
        // 비정상 상황 시 0번 섹터라도 반환하여 크래시 방지
        return *m_sectors[0];
    }

    return *m_sectors[index];
}

Sector& SectorManager::GetSectorByTile(int tileX, int tileY) {
    Pos idx = GetSectorIndex(tileX, tileY);
    return GetSector(idx.x, idx.y);
}

void SectorManager::GetNearbySessions(int x, int y, std::vector<Session*>& outSessions) {
    Pos center = GetSectorIndex(x, y);

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int nx = center.x + dx;
            int ny = center.y + dy;

            // 범위 체크
            if (nx >= 0 && nx < m_sectorCountX && ny >= 0 && ny < m_sectorCountY) {
                Sector& sector = GetSector(nx, ny);
                std::lock_guard<std::mutex> lock(sector.sectorMutex);

                if (!sector.sessions.empty()) {
                    outSessions.insert(outSessions.end(), sector.sessions.begin(), sector.sessions.end());
                }
            }
        }
    }
}