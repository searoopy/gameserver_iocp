#pragma once

#include <vector>
#include <atomic>

#define MAX_TILE_X 30
#define MAX_TILE_Y 30


enum class ENUM_TILE_NAME {
	empty = 0,
	wall = 1,
	player = 2,
	monster = 3
};

using MapGrid = std::vector<std::vector<ENUM_TILE_NAME>>;

class TileMgr
{
public:
	TileMgr();
	~TileMgr();


	void GenerateMap();
	const MapGrid& GetMap() const { return map; }
	bool IsWall(int x, int y);


	void SetOccupied(int nextX, int nextY, ENUM_TILE_NAME state);
	bool IsOccupied(int nextX, int nextY);

	bool IsWalkable(int nextX, int nextY);

private:
	//char tile[MAX_TILE_Y][MAX_TILE_X];
	MapGrid map;
	std::atomic<ENUM_TILE_NAME> tile_atomic[MAX_TILE_Y][MAX_TILE_X];
	//MapGrid _mapUse;

};

extern TileMgr g_tileMgr;