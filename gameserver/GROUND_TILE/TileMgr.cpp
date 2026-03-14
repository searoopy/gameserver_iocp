#include "TileMgr.h"
#include <random>
#include "..\Debug.h"
#include <fstream>
#include <nlohmann/json.hpp>


//TileMgr g_ptileMgr;
std::unique_ptr <TileMgr> g_pTileMgr ;

using json = nlohmann::json;

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
   



  /*  map.assign(MAX_TILE_Y, std::vector< ENUM_TILE_NAME  >(MAX_TILE_X));
    for (int y = 0; y < MAX_TILE_Y; ++y) {
        for (int x = 0; x < MAX_TILE_X; ++x) {
            map[y][x] = static_cast<ENUM_TILE_NAME>(g_TestMap[y][x]);
            tile_atomic[y][x] = static_cast<ENUM_TILE_NAME>(g_TestMap[y][x]);
        }
    }*/

#ifdef _DEBUG
    std::ifstream file("../data/map_data.json");
#else
    std::ifstream file("data/map_data.json");
#endif

    if (!file.is_open()) {

        LOG("MAP 파일을 열수 없습니다");
        __debugbreak();
        return;
    }
    

    try {
        json mapData;
        file >> mapData;

        // JSON에서 가로, 세로 크기 가져오기 (제공된 데이터는 float 형태이므로 int 변환)
        int jsonWidth = static_cast<int>(mapData["width"]);
        int jsonHeight = static_cast<int>(mapData["height"]);

        map_size_x = jsonWidth;
        map_size_y = jsonHeight;
        tile_atomic = std::vector<std::atomic<ENUM_TILE_NAME>>(map_size_x * map_size_y);

        map.assign(map_size_y, std::vector<ENUM_TILE_NAME>(map_size_x, ENUM_TILE_NAME::empty));

        // JSON의 data 배열 접근
        const auto& dataArray = mapData["data"];

        for (int y = 0; y < jsonHeight; ++y) {
            // 배열 범위를 넘지 않도록 체크
            if (y >= map_size_y) break;

            for (int x = 0; x < jsonWidth; ++x) {
                if (x >= map_size_x) break;

                // JSON 배열에서 값을 가져옴 (제공 데이터가 1.0 같은 float이므로 double로 받음)
                double tileValue = dataArray[y][x];

                // atomic 배열에 저장 (store 사용)
                // static_cast를 통해 enum 타입으로 변환

                map[y][x] = static_cast<ENUM_TILE_NAME>(tileValue);
                tile_atomic[GetTileIdx(x,y)] = static_cast<ENUM_TILE_NAME>(tileValue);
               // tile_atomic[y][x].store(static_cast<ENUM_TILE_NAME>(static_cast<int>(tileValue)));
            }
        }

        //std::cout << "성공: " << jsonWidth << "x" << jsonHeight << " 데이터를 로드했습니다." << std::endl;
        LOG("MAP파일 읽기 성공\n");

    }
    catch (const json::exception& e) {
        LOG("MAP파일 읽기 실패\n");
        //std::cerr << "JSON 로드 중 오류 발생: " << e.what() << std::endl;
    }

}

bool TileMgr::IsWall(int x, int y) 
{
   // if (x < 0 || x >= MAX_TILE_X || y < 0 || y >= MAX_TILE_Y) return true;
   // return tile[y][x] == 1;
    if (x < 0 || x >= map_size_x || y < 0 || y >= map_size_y) return true;
    return map[y][x] == ENUM_TILE_NAME::wall;
}

void TileMgr::SetOccupied(int nextX, int nextY, ENUM_TILE_NAME value)
{

    if (nextX < 0 || nextX >= map_size_x || nextY < 0 || nextY >= map_size_y)
    {
        LOG("SetOccupied out of index\n");
        return;
    }

   // if (map[nextY][nextX] != ENUM_TILE_NAME::wall)
    if (tile_atomic[GetTileIdx(nextX, nextY)] != ENUM_TILE_NAME::wall)
        tile_atomic[GetTileIdx(nextX, nextY)] = value;
}

bool TileMgr::TryOccupy(int x, int y, ENUM_TILE_NAME state)
{
    if (x < 0 || x >= map_size_x || y < 0 || y >= map_size_y)
    {
        LOG("SetOccupied out of index\n");
        return false;
    }

    ENUM_TILE_NAME expected = ENUM_TILE_NAME::empty;

    // compare_exchange_strong(예상값, 바꿀값)
    // 1. tile_atomic[y][x]가 'empty'인지 확인한다.
    // 2. 'empty'가 맞다면 즉시 'myState'로 바꾸고 true 반환.
    // 3. 누군가 이미 선점했다면 false 반환.
    return tile_atomic[GetTileIdx(x, y)].compare_exchange_strong(expected, state);


}

bool TileMgr::IsOccupied(int nextX, int nextY)
{
    if (nextX < 0 || nextX >= map_size_x || nextY < 0 || nextY >= map_size_y)
    {
        LOG("IsOccupied out of index\n");
        return true ;
    }


    return (tile_atomic[GetTileIdx(nextX, nextY)] == ENUM_TILE_NAME::monster ||
        tile_atomic[GetTileIdx(nextX, nextY)] == ENUM_TILE_NAME::wall ||
        tile_atomic[GetTileIdx(nextX, nextY)] == ENUM_TILE_NAME::player);


}

bool TileMgr::IsWalkable(int nextX, int nextY)
{
    if (nextX < 0 || nextX >= map_size_x || nextY < 0 || nextY >= map_size_y)
    {
        LOG("IsWalkable out of index\n");
        return false;
    }

    return  map[nextX][nextY] == ENUM_TILE_NAME::empty;
   // return false;
}


