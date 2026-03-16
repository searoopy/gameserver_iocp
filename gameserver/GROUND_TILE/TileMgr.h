#pragma once

#include <vector>
#include <atomic>
#include <memory>

//#define MAX_TILE_X 30
//#define MAX_TILE_Y 30


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


private:
	void GenerateMap();

public:
	const MapGrid& GetMap() const { return map; }
	bool IsWall(int x, int y);


	void SetOccupied(int nextX, int nextY, ENUM_TILE_NAME state);
	bool TryOccupy(int x, int y, ENUM_TILE_NAME state);
	bool IsOccupied(int nextX, int nextY);

	bool IsWalkable(int nextX, int nextY);


	inline int GetTileIdx(int x, int y) { return y * map_size_x + x;  }
	inline int MapSizeX() { return map_size_x; }
	inline int MapSizeY() { return map_size_y; }


private:
	//char tile[MAX_TILE_Y][MAX_TILE_X];
	MapGrid map;
	//MapGrid map_grid;


	std::vector<std::atomic<ENUM_TILE_NAME>> tile_atomic;
	//std::atomic<ENUM_TILE_NAME> tile_atomic[MAX_TILE_Y][MAX_TILE_X];
	//MapGrid _mapUse;


	int map_size_x;
	int map_size_y;

};

extern std::unique_ptr<TileMgr> g_pTileMgr;