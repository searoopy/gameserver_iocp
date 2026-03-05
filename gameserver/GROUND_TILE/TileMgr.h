#pragma once

#include <vector>

#define MAX_TILE_X 30
#define MAX_TILE_Y 30
#define TILE_SIZE 10

using MapGrid = std::vector<std::vector<int>>;

class TileMgr
{
public:
	TileMgr();
	~TileMgr();


	void GenerateMap();
	const MapGrid& GetMap() const { return _map; }
	bool IsWall(int x, int y);

private:
	//char tile[MAX_TILE_Y][MAX_TILE_X];
	MapGrid _map;

};

extern TileMgr g_tileMgr;