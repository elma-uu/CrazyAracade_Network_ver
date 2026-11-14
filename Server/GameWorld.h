#pragma once

#include <cstdint>
#include <vector>
#include "ServerBomb.h"
#include "FixedStepLoop.h"

/* 수정 - 2026-11-07 */
#include "../Common/protocol.h"

struct Board {
    std::uint8_t obstacle = 0;
    std::uint8_t variant = 0;
    std::uint8_t hiddenItem = 0;
};

class GameWorld {
public:
    static constexpr int MapTiles = MAP_TILES;
    void BuildMapInfo(S2C_MapInfo& out) const;
    // 방이 넘겨준 경과 시간만큼 이동과 물풍선을 갱신한다.
    template<class Move>
    void Advance(std::uint64_t elapsedMs, Move stepMovement)
    {
        clock.Advance(elapsedMs, stepMovement, [this] { StepBomb(); });
    }
    void StepBomb();
    void ResolveExplosion(ServerBomb& bomb);
    bool InitBoard(MAP_ID id);
    bool IsWalkable(int row, int col) const
    {
        return row >= 0 && row < MapTiles && col >= 0 && col < MapTiles
            && board[row][col].obstacle == 0;
    }

private:
    FixedStepLoop clock;
    std::vector<ServerBomb> bombs;
    Board board[MapTiles][MapTiles]{};
    MAP_ID mapId = MAP_FOREST;
};
