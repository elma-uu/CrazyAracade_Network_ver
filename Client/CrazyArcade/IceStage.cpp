#include "IceStage.h"
#include "Gfx.h"

namespace {
    // 2 = 부서지지 않는 얼음 (3x3 얼음 덩어리 두 개는 고정 위치에 따로 그림)
    const TileMap::Layout kLayout = {
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
}

void IceStage::BuildMap(TileMap& map)
{
    map.Load(kLayout);
    // 빈 칸의 약 40%를 무작위 얼음 블록으로 채운다
    for (int row = 0; row < TileMap::Size; ++row) {
        for (int col = 0; col < TileMap::Size; ++col) {
            Tile& tile = map.At(row, col);
            if (tile.obstacle != Obstacle::Empty || rand() % 10 <= 5) continue;
            tile.obstacle = Obstacle::Box;
            tile.variant = rand() % 2;
            if (rand() % 10 > 5) tile.item = Item::Random();
        }
    }
    // 시작 위치는 비워 둔다
    for (int i = 0; i < PlayerCount; ++i) {
        POINT spawn = GetSpawnTile(i);
        map.At(spawn.y, spawn.x) = Tile{};
    }
}

POINT IceStage::GetSpawnTile(int playerIndex) const
{
    return playerIndex == 0 ? POINT{ 6, 5 } : POINT{ 13, 5 };
}

void IceStage::OnPlayerHit(Player& victim, Player& attacker)
{
    if (--mHearts[victim.GetIndex()] <= 0) EndGame(attacker.GetIndex());
}

void IceStage::RenderBackground(HDC hdc)
{
    Gfx::DrawOpaque(hdc, BitmapID::IceTileMax, Config::MapOriginX, Config::MapOriginY, 800, 800, 0, 0, 1400, 1400);
}

void IceStage::RenderMap(HDC hdc)
{
    // 3x3 얼음 덩어리
    for (int col : { 5, 12 }) {
        POINT c = TileMap::TileCenter(2, col);
        Gfx::Draw(hdc, BitmapID::IceObstacles, c.x - Config::TileHalf, c.y - Config::TileHalf, 120, 120, 160, 0, 120, 120);
    }

    for (int row = 0; row < TileMap::Size; ++row) {
        for (int col = 0; col < TileMap::Size; ++col) {
            const Tile& tile = mMap.At(row, col);
            POINT c = TileMap::TileCenter(row, col);

            mMap.RenderItemAt(hdc, row, col);
            RenderPlayersAt(hdc, row, col);

            if (tile.obstacle != Obstacle::Box) continue;
            int left = c.x - Config::TileHalf;
            if (tile.variant == 0)
                Gfx::Draw(hdc, BitmapID::IceBlock1, left, c.y - 45, 40, 70, 0, 0, 42, 54);
            else
                Gfx::Draw(hdc, BitmapID::IceBlock2, left, c.y - 35, 40, 60, 0, 0, 42, 48);
        }
    }
}

void IceStage::RenderHud(HDC hdc)
{
    const int startX[PlayerCount] = { 10, 1170 };
    for (int i = 0; i < PlayerCount; ++i)
        for (int h = 0; h < mHearts[i]; ++h)
            Gfx::Draw(hdc, BitmapID::Heart, startX[i] + h * 90, 250, 80, 64, 0, 0, 80, 64);
}
