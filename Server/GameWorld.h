#pragma once

#include <cstdint>

/* 수정 - 2026-11-07 */
enum class MapId : std::uint8_t { Forest = 1, City, Ice };

struct Board {
    std::uint8_t obstacle = 0;
    std::uint8_t variant = 0;
    std::uint8_t hiddenItem = 0;
};

class GameWorld {
public:
    static constexpr int MapTiles = 20;
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
