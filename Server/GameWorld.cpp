#include "GameWorld.h"
#include <cstdlib>
#include <algorithm>
#include <initializer_list>

namespace {
constexpr unsigned char forestLayout[GameWorld::MapTiles][GameWorld::MapTiles] = {
        {0,0,1,1,0,1,0,0,0,0,0,0,0,0,1,0,1,1,0,0},
        {0,0,0,1,0,0,0,0,0,2,2,0,0,0,0,0,1,0,0,0},
        {1,0,0,0,0,0,1,0,0,2,2,0,0,1,0,0,0,0,0,1},
        {0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0,0,0},
        {0,0,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1,1,0,0},
        {0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0},
        {0,1,0,0,1,0,0,0,0,1,1,0,0,0,0,1,0,0,1,0},
        {1,1,1,0,1,1,1,0,0,1,1,0,0,1,1,1,0,1,1,1},
        {1,1,1,1,1,1,0,0,1,1,1,1,0,0,1,1,1,1,1,1},
        {1,1,1,1,1,1,0,0,1,1,1,1,0,0,1,1,1,1,1,1},
        {1,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,0,1},
        {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0},
        {0,0,1,0,0,1,0,0,0,1,1,0,0,0,1,0,0,1,0,0},
        {0,1,1,1,0,1,1,0,0,0,0,0,0,1,1,0,1,1,1,0},
        {0,0,0,1,1,1,1,0,1,1,1,1,0,1,1,1,1,0,0,0},
        {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
        {0,0,0,0,1,0,0,1,1,1,1,1,1,0,0,1,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0},
        {1,1,0,0,0,0,0,0,3,1,1,3,0,0,0,0,0,0,1,1},
    };

constexpr unsigned char cityLayout[GameWorld::MapTiles][GameWorld::MapTiles] = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,2,2,0,0,0,0,0,0,0,0,0,0,2,2,0,0,0},
        {0,2,2,2,2,0,0,0,0,0,0,0,0,0,0,2,2,0,0,0},
        {0,2,2,4,2,2,2,0,0,0,0,0,0,2,2,4,2,2,2,0},
        {0,3,2,0,0,3,2,1,1,0,0,1,1,5,2,0,0,5,2,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,6,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,6,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,2,2,0,0,0,0,0,0,0,0,0,0,2,2,0,0,0},
        {0,2,2,2,2,0,0,0,0,0,0,0,0,0,0,2,2,0,0,0},
        {0,2,2,4,2,2,2,0,0,0,0,0,0,2,2,4,2,2,2,0},
        {0,3,2,0,0,3,2,1,1,0,0,1,1,5,2,0,0,5,2,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    };

constexpr unsigned char iceLayout[GameWorld::MapTiles][GameWorld::MapTiles] = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,2,2,2,0,0,0,0,2,2,2,0,0,0,0,0},
        {0,0,0,0,0,2,2,2,0,0,0,0,2,2,2,0,0,0,0,0},
        {0,0,0,0,0,2,2,2,0,0,0,0,2,2,2,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0},
        {0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,2,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0},
        {0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,2,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    };

struct Placement { int row, col, variant; };
    const Placement cityVariants[] = {
        { 4, 7, 1 },  { 4, 8, 2 },  { 4, 11, 2 },  { 4, 12, 1 },
        { 18, 7, 1 }, { 18, 8, 2 }, { 18, 11, 2 }, { 18, 12, 1 },
    };
}

/* 수정 - 2026-11-09 */
bool GameWorld::InitBoard(MapId id)
{
    const unsigned char (*layout)[MapTiles] = nullptr;
    switch (id) {
    case MapId::Forest: layout = forestLayout; break;
    case MapId::City: layout = cityLayout; break;
    case MapId::Ice: layout = iceLayout; break;
    default: return false;
    }
    bombs.clear();
    mapId = id;
    for (int row = 0; row < MapTiles; ++row) {
        for (int col = 0; col < MapTiles; ++col) {
            Board& tile = board[row][col];
            tile = {};
            tile.obstacle = layout[row][col];
            if (id == MapId::Forest && tile.obstacle == 3)
                tile.obstacle = 0; // 기존 2인용 바구니 제거
            if (id == MapId::Ice && tile.obstacle == 0 && std::rand() % 10 > 5)
                tile.obstacle = 1;
            if (id != MapId::City && tile.obstacle == 1) {
                tile.variant = std::rand() % (id == MapId::Forest ? 5 : 2);
                if (std::rand() % 10 > 5)
                    tile.hiddenItem = 1 + std::rand() % 3;
            }
        }
    }
    if (id == MapId::City) {
        for (const auto& p : cityVariants)
            board[p.row][p.col].variant = p.variant;
    }
    // 네 모서리에서 시작하고 옆 칸으로 나올 수 있게 한다.
    for (int row : {0, MapTiles - 1}) {
        for (int col : {0, MapTiles - 1}) {
            int innerRow = row == 0 ? 1 : row - 1;
            int innerCol = col == 0 ? 1 : col - 1;
            board[row][col] = {};
            board[innerRow][col] = {};
            board[row][innerCol] = {};
            if (id == MapId::Forest)
                board[innerRow][col].obstacle = 3;
        }
    }
    return true;
}

/* 수정 - 2026-11-10 */
void GameWorld::BuildMapInfo(S2C_MapInfo& out) const
{
    out.size = sizeof(out);
    out.type = S2C_MAP_INFO;
    out.mapId = mapId;
    for (int row = 0; row < MapTiles; ++row) {
        for (int col = 0; col < MapTiles; ++col) {
            out.obstacle[row][col] = board[row][col].obstacle;
            out.obstacleType[row][col] = board[row][col].variant;
        }
    }
}

/* 수정 - 2026-11-12 */
// 서버가 50ms마다 한 번 호출한다.
void GameWorld::StepBomb()
{
    for (ServerBomb& bomb : bombs)
        bomb.Tick();

    bombs.erase(std::remove_if(bombs.begin(), bombs.end(),
        [](const ServerBomb& bomb) { return bomb.state == ServerBomb::Removed; }),
        bombs.end());
}
