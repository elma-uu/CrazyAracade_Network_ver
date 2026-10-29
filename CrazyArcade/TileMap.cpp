#include "TileMap.h"

void TileMap::Load(const Layout& obstacles)
{
    for (int row = 0; row < Size; ++row) {
        for (int col = 0; col < Size; ++col) {
            Tile& tile = mTiles[row][col];
            tile.obstacle = obstacles[row][col];
            tile.variant = 0;
            tile.item = ItemType::None;
        }
    }
}

void TileMap::RenderItemAt(HDC hdc, int row, int col) const
{
    const Tile& tile = mTiles[row][col];
    if (tile.obstacle != Obstacle::Empty) return;
    if (const Item* item = Item::Get(tile.item)) {
        POINT c = TileCenter(row, col);
        item->Render(hdc, c.x, c.y - 10);
    }
}
