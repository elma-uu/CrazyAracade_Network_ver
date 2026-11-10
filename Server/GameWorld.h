#pragma once

#include <cstdint>

/* 수정 - 2026-11-07 */
#include "../Shared/MapInfo.h"

struct Board {
    std::uint8_t obstacle = 0;
    std::uint8_t variant = 0;
    std::uint8_t hiddenItem = 0;
};

class GameWorld {
public:
    static constexpr int MapTiles = MAP_TILES;
    void BuildMapInfo(S2C_MapInfo& out) const;
    bool InitBoard(MapId id);
    bool IsWalkable(int row, int col) const
    {
        return row >= 0 && row < MapTiles && col >= 0 && col < MapTiles
            && board[row][col].obstacle == 0;
    }

private:
    Board board[MapTiles][MapTiles]{};
    MapId mapId = MapId::Forest;
};
