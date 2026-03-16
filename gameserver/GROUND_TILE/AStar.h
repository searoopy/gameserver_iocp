#pragma once

#include <vector>
#include <queue>
#include <deque>
#include <cmath>
#include <algorithm>
#include "TileMgr.h"


struct Pos
{
    Pos()
    : x(0), y(0)
    {

    }

    Pos(int _x, int _y)
        :x(_x), y(_y)
    {

    }

    int x, y;
    bool operator==(const Pos& other) const { return x == other.x && y == other.y; }
};



struct Node
{
	Pos pos;
	int g, h; // g: 시작점부터의 비용, h :목적지까지의 예상비용(휴리스틱)
	Pos parent;

	int f() const { return g + h;  }

	bool operator>(const Node& other) const { return f() > other.f(); }

};


class AStar 
{
public:
	//맵 데이터와 시작/목적지를 받아 경로 반환.
    static std::deque<Pos> FindPath(const std::vector<std::vector<ENUM_TILE_NAME>>& map, Pos start, Pos dest)
    {
        int height = map.size();
        int width = map[0].size();

        // 시작점과 끝점이 같으면 바로 리턴
        if (start == dest) return {};

        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openList;
        std::vector<std::vector<bool>> closedList(height, std::vector<bool>(width, false));
        std::vector<std::vector<Pos>> parentMap(height, std::vector<Pos>(width, { -1, -1 }));

        // G 값을 추적하기 위한 배열 (더 좋은 경로 발견 시 갱신용)
        std::vector<std::vector<int>> gScore(height, std::vector<int>(width, INT_MAX));

        gScore[start.y][start.x] = 0;
        openList.push({ start, 0, Heuristic(start, dest), {-1,-1} });

        while (!openList.empty())
        {
            Node current = openList.top();
            openList.pop();

            if (closedList[current.pos.y][current.pos.x]) continue;
            closedList[current.pos.y][current.pos.x] = true;

            if (current.pos == dest) return ReconstructPath(parentMap, dest);

            // 8방향 탐색
            Pos dirs[8] = { {0, 1}, {0, -1}, {1, 0}, {-1, 0}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1} };

            for (int i = 0; i < 8; ++i)
            { 
                Pos d = dirs[i];
                Pos next = { current.pos.x + d.x, current.pos.y + d.y };

                // 1. 경계 체크
                if (next.x < 0 || next.x >= width || next.y < 0 || next.y >= height) continue;
                // 2. 벽 체크
                if (g_pTileMgr->IsWall(next.x, next.y)) continue;
                //if (map[next.y][next.x] != ENUM_TILE_NAME::empty) continue;
                // 3. 방문 체크
                if (closedList[next.y][next.x]) continue;

                // [추가] 대각선 코너 체크: 대각선 이동 시 양옆이 벽이면 통과 못함
                if (i >= 4) { // 대각선 방향들
                    if (map[current.pos.y][next.x] != ENUM_TILE_NAME::empty ||
                        map[next.y][current.pos.x] != ENUM_TILE_NAME::empty) continue;
                }

                // 4. 비용 계산 (상하좌우 10, 대각선 14)
                int weight = (i < 4) ? 10 : 14;
                int nextG = current.g + weight;

                // 5. 더 좋은 경로를 찾은 경우에만 진행
                if (nextG < gScore[next.y][next.x]) {
                    gScore[next.y][next.x] = nextG;
                    parentMap[next.y][next.x] = current.pos;
                    openList.push({ next, nextG, Heuristic(next, dest) * 10, current.pos });
                }
            }
        }
        return {};
    }


private:

	static int Heuristic(Pos a, Pos b) {
		//return std::abs(a.x - b.x) + std::abs(a.y - b.y); // Manhattan Distance

        int dx = abs(a.x - b.x);
        int dy = abs(a.y - b.y);
        // 상하좌우 비용 10, 대각선 비용 14 기준
        // (직선 거리 차이의 최소값만큼 대각선 이동) + (남은 직선 거리)

        int minVal = std::min<int>(dx, dy);

        return 10 * (dx + dy) + (14 - 2 * 10) * minVal;

	}


	static std::deque<Pos> ReconstructPath(const std::vector<std::vector<Pos>>& parentMap, Pos current)
	{
		std::deque<Pos> path;
		while (current.x != -1)
		{
			path.push_back(current);
			current = parentMap[current.y][current.x];
		}

		std::reverse(path.begin(), path.end());
		return path;
	}

};