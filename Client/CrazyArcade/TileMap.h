#pragma once
#include "Common.h"
#include "Item.h"

// 모든 스테이지 공통 장애물 코드. 그 외 값(2~8)은 스테이지별로 의미가 다르다.
namespace Obstacle {
    constexpr int Empty = 0;
    constexpr int Box = 1;   // 물줄기로 부서지는 블록
    constexpr int Bomb = 9;  // 설치된 물풍선 (통행 불가)
}

struct Tile {
    int obstacle = Obstacle::Empty;
    int variant = 0;                 // 같은 장애물의 그림 종류
    ItemType item = ItemType::None;  // 바닥(또는 블록 안)에 놓인 아이템
};

class TileMap {
public:
    static constexpr int Size = Config::MapTiles;
    using Layout = int[Size][Size];

    void Load(const Layout& obstacles);

    Tile& At(int row, int col) { return mTiles[row][col]; }
    const Tile& At(int row, int col) const { return mTiles[row][col]; }

    static bool InBounds(int row, int col) { return row >= 0 && row < Size && col >= 0 && col < Size; }
    bool IsWalkable(int row, int col) const { return InBounds(row, col) && mTiles[row][col].obstacle == Obstacle::Empty; }

    // 타일 중심의 화면 좌표
    static POINT TileCenter(int row, int col)
    {
        return { Config::MapOriginX + col * Config::TileSize + Config::TileHalf,
                 Config::MapOriginY + row * Config::TileSize + Config::TileHalf };
    }

    // 바닥에 드러난 아이템을 타일 위치에 그린다
    void RenderItemAt(HDC hdc, int row, int col) const;

private:
    Tile mTiles[Size][Size];
};
