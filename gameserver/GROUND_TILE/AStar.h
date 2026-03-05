#pragma once

#include <vector>
#include <queue>
#include <deque>
#include <cmath>
#include <algorithm>


struct Pos
{
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
	static std::deque<Pos> FindPath(const std::vector<std::vector<int>>& map, Pos start, Pos dest)
	{
		int height = map.size();
		int width = map[0].size();


		std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openList;
		std::vector<std::vector<bool>> closedList(height, std::vector<bool>(width, false));
		std::vector<std::vector<Pos>> parentMap(height, std::vector<Pos>(width, { -1, -1 }));



		openList.push({ start, 0, Heuristic(start, dest), {-1,-1 } });


		while (!openList.empty())
		{
			Node current = openList.top();
			openList.pop();

			if (closedList[current.pos.y][current.pos.x]) continue;
			closedList[current.pos.y][current.pos.x] = true;


			//목적지 도착시 경로 복원
			if (current.pos == dest)
			{
				return ReconstructPath(parentMap, dest);
			}


			// 상하좌우 탐색 (필요 시 대각선 추가 가능)
			Pos dirs[4] = { {0, 1}, {0, -1}, {1, 0}, {-1, 0} };
			for (auto& d : dirs) {
				Pos next = { current.pos.x + d.x, current.pos.y + d.y };

				if (next.x >= 0 && next.x < width && next.y >= 0 && next.y < height &&
					map[next.y][next.x] == 0 && !closedList[next.y][next.x]) {

					int nextG = current.g + 1;
					parentMap[next.y][next.x] = current.pos;
					openList.push({ next, nextG, Heuristic(next, dest), current.pos });
				}
			}


		}

		return {}; //경로 없음.

	}


private:

	static int Heuristic(Pos a, Pos b) {
		return std::abs(a.x - b.x) + std::abs(a.y - b.y); // Manhattan Distance
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