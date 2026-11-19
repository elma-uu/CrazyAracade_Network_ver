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
bool GameWorld::InitBoard(MAP_ID id)
{
    const unsigned char (*layout)[MapTiles] = nullptr;
    switch (id) {
    case MAP_FOREST: layout = forestLayout; break;
    case MAP_CITY: layout = cityLayout; break;
    case MAP_ICE: layout = iceLayout; break;
    default: return false;
    }
    clock = FixedStepLoop{};
    bombs.clear();
    nextBombId = 0;
    mapId = id;
    for (int row = 0; row < MapTiles; ++row) {
        for (int col = 0; col < MapTiles; ++col) {
            Board& tile = board[row][col];
            tile = {};
            tile.obstacle = layout[row][col];
            if (id == MAP_FOREST && tile.obstacle == 3)
                tile.obstacle = 0; // 기존 2인용 바구니 제거
            if (id == MAP_ICE && tile.obstacle == 0 && std::rand() % 10 > 5)
                tile.obstacle = 1;
            if (id != MAP_CITY && tile.obstacle == 1) {
                tile.variant = std::rand() % (id == MAP_FOREST ? 5 : 2);
                if (std::rand() % 10 > 5)
                    tile.hiddenItem = 1 + std::rand() % 3;
            }
        }
    }
    if (id == MAP_CITY) {
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
            if (id == MAP_FOREST)
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
    StepBomb({});
}

/* 수정 - 2026-11-19 */
void GameWorld::StepBomb(const std::function<void(const S2C_Explode&)>& broadcast)
{
    for (ServerBomb& bomb : bombs) {
        if (bomb.Tick()) {
            ResolveExplosion(bomb);
            S2C_Explode packet{};
            packet.size = sizeof(packet);
            packet.type = S2C_EXPLODE;
            packet.bombId = bomb.id;
            packet.reachLeft = static_cast<unsigned char>(bomb.reachL);
            packet.reachRight = static_cast<unsigned char>(bomb.reachR);
            packet.reachUp = static_cast<unsigned char>(bomb.reachU);
            packet.reachDown = static_cast<unsigned char>(bomb.reachD);
            if (broadcast) broadcast(packet);
        }
    }

    bombs.erase(std::remove_if(bombs.begin(), bombs.end(),
        [](const ServerBomb& bomb) { return bomb.state == ServerBomb::Removed; }),
        bombs.end());
}

/* 수정 - 2026-11-13 */
void GameWorld::ResolveExplosion(ServerBomb& bomb)
{
    constexpr int empty = 0, box = 1, balloon = 9;
    board[bomb.row][bomb.col].obstacle = empty;

    auto spread = [&](int dRow, int dCol) {
        int distance = 0;
        for (int step = 1; step <= bomb.reach; ++step) {
            int row = bomb.row + step * dRow;
            int col = bomb.col + step * dCol;
            if (row < 0 || row >= MapTiles || col < 0 || col >= MapTiles) break;

            Board& tile = board[row][col];
            if (tile.obstacle == box) {
                tile.obstacle = empty; // 상자 안의 아이템은 해당 칸에 남는다.
                tile.variant = 0;
                distance = step;
                break;
            }
            if (tile.obstacle != empty && tile.obstacle != balloon) break;
            distance = step;
        }
        return distance;
    };

    bomb.reachU = spread(-1, 0);
    bomb.reachD = spread(1, 0);
    bomb.reachL = spread(0, -1);
    bomb.reachR = spread(0, 1);
}

/* 수정 - 2026-11-16 */
bool GameWorld::PlaceBomb(const C2S_Attack& request, const BombOwner& owner)
{
    return PlaceBomb(request, owner, nullptr);
}

bool GameWorld::PlaceBomb(const C2S_Attack& request, const BombOwner& owner, S2C_Attack* placed)
{
    if (request.size != sizeof(request) || request.type != C2S_ATTACK)
        return false;
    if (!owner.alive || owner.slot < 0 || owner.slot >= MAX_ROOM_PLAYER
        || owner.reach < 1 || owner.reach > 255 || !IsWalkable(owner.row, owner.col))
        return false;

    int limit = owner.maxBombs;
    if (mapId == MAP_FOREST) limit -= owner.apples;
    auto owned = std::count_if(bombs.begin(), bombs.end(),
        [&](const ServerBomb& bomb) { return bomb.ownerSlot == owner.slot; });
    if (owned >= limit) return false;

    bombs.push_back({nextBombId, owner.slot, owner.row, owner.col, owner.reach});
    if (placed) {
        placed->size = sizeof(*placed);
        placed->type = S2C_ATTACK;
        placed->bombId = nextBombId;
        placed->x = static_cast<unsigned char>(owner.col);
        placed->y = static_cast<unsigned char>(owner.row);
        placed->reach = static_cast<unsigned char>(owner.reach);
    }
    ++nextBombId;
    board[owner.row][owner.col].obstacle = 9;
    return true;
}
