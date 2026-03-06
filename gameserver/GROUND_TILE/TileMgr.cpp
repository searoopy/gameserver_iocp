#include "TileMgr.h"
#include <random>
#include "..\Debug.h"

TileMgr g_tileMgr;


const char g_TestMap[MAX_TILE_Y][MAX_TILE_X] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, // 00
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, // 01
    {1,0,1,1,1,0,1,1,1,0,1,1,1,0,0,0,1,1,1,0,1,1,1,0,1,1,1,0,0,1}, // 02
    {1,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,1}, // 03
    {1,0,1,0,1,1,1,0,1,0,1,1,1,0,1,0,1,1,1,0,1,0,1,1,1,0,1,0,0,1}, // 04
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, // 05
    {1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,0,1}, // 06
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, // 07
    {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,0,1}, // 08
    {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,0,1}, // 09
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, // 10
    {1,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,1}, // 11
    {1,0,1,1,1,0,0,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,0,1}, // 12
    {1,0,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,1}, // 13
    {1,0,0,0,0,0,0,1,0,0,0,1,0,0,1,1,0,0,1,0,0,0,1,0,0,0,0,0,0,1}, // 14
    {1,0,0,0,0,0,0,1,1,0,1,1,0,0,1,1,0,0,1,1,0,1,1,0,0,0,0,0,0,1}, // 15
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, // 16
    {1,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,1}, // 17
    {1,0,1,1,1,0,0,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,0,1}, // 18
    {1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1}, // 19
    {1,0,1,0,1,0,1,0,1,0,1,0,0,1,0,0,1,0,0,1,0,1,0,1,0,1,0,1,0,1}, // 20
    {1,0,1,0,1,0,1,0,1,0,1,0,0,1,0,0,1,0,0,1,0,1,0,1,0,1,0,1,0,1}, // 21
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, // 22
    {1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,0,1}, // 23
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, // 24
    {1,0,1,0,1,1,1,0,1,0,1,1,1,0,1,0,1,1,1,0,1,0,1,1,1,0,1,0,0,1}, // 25
    {1,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,1}, // 26
    {1,0,1,1,1,0,1,1,1,0,1,1,1,0,0,0,1,1,1,0,1,1,1,0,1,1,1,0,0,1}, // 27
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, // 28
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}  // 29
};

TileMgr::TileMgr()
{
    // 랜덤 시드 초기화
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    GenerateMap();

}

TileMgr::~TileMgr()
{
}



void TileMgr::GenerateMap()
{

    //for (int y = 0; y < MAX_TILE_Y; ++y) {
    //    for (int x = 0; x < MAX_TILE_X; ++x) {
    //        if (y < 6) tile[y][x] = g_TestMap[y][x]; // 정의된 부분 복사
    //        else if (x == 0 || x == MAX_TILE_X-1 || y == MAX_TILE_Y-1) tile[y][x] = 1; // 테두리만 벽
    //        else tile[y][x] = 0; // 나머지는 빈 공간
    //    }
    //}

    map.assign(MAX_TILE_Y, std::vector< ENUM_TILE_NAME  >(MAX_TILE_X));
    for (int y = 0; y < MAX_TILE_Y; ++y) {
        for (int x = 0; x < MAX_TILE_X; ++x) {
            map[y][x] = static_cast<ENUM_TILE_NAME>(g_TestMap[y][x]);
            tile_atomic[y][x] = static_cast<ENUM_TILE_NAME>(g_TestMap[y][x]);
        }
    }

}

bool TileMgr::IsWall(int x, int y) 
{
   // if (x < 0 || x >= MAX_TILE_X || y < 0 || y >= MAX_TILE_Y) return true;
   // return tile[y][x] == 1;
    if (x < 0 || x >= MAX_TILE_X || y < 0 || y >= MAX_TILE_Y) return true;
    return map[y][x] == ENUM_TILE_NAME::wall;
}

void TileMgr::SetOccupied(int nextX, int nextY, ENUM_TILE_NAME value)
{

    if (nextX < 0 || nextX >= MAX_TILE_X || nextY < 0 || nextY >= MAX_TILE_Y)
    {
        LOG("SetOccupied out of index\n");
        return;
    }

   // if (map[nextY][nextX] != ENUM_TILE_NAME::wall)
    if (tile_atomic[nextY][nextX] != ENUM_TILE_NAME::wall)
        tile_atomic[nextY][nextX] = value;
}

bool TileMgr::IsOccupied(int nextX, int nextY)
{
    if (nextX < 0 || nextX >= MAX_TILE_X || nextY < 0 || nextY >= MAX_TILE_Y)
    {
        LOG("IsOccupied out of index\n");
        return true ;
    }


    return (tile_atomic[nextY][nextX] == ENUM_TILE_NAME::monster ||
        tile_atomic[nextY][nextX] == ENUM_TILE_NAME::wall ||
        tile_atomic[nextY][nextX] == ENUM_TILE_NAME::player);


}

bool TileMgr::IsWalkable(int nextX, int nextY)
{
    if (nextX < 0 || nextX >= MAX_TILE_X || nextY < 0 || nextY >= MAX_TILE_Y)
    {
        LOG("IsWalkable out of index\n");
        return false;
    }

    return  map[nextX][nextY] == ENUM_TILE_NAME::empty;
   // return false;
}


