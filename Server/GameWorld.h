#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include "ServerBomb.h"
#include "FixedStepLoop.h"

/* 수정 - 2026-11-07 */
#include "../Common/protocol.h"

struct Board {
    std::uint8_t obstacle = 0;
    std::uint8_t variant = 0;
    std::uint8_t hiddenItem = 0;
};

// 공격을 요청한 세션의 서버 측 플레이어 상태
struct BombOwner {
    int slot, row, col;
    bool alive;
    int reach = 1, maxBombs = 2, apples = 0;
};

struct ItemOwner : BombOwner {
    float speed = 1.0f;
    int timeLeftMs = 30000;
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
    bool PlaceBomb(const C2S_Attack& request, const BombOwner& owner);
    /* 수정 - 2026-11-17 */
    template<class Room>
    bool OnAttack(Room& room, int sessionId, const C2S_Attack& request, const BombOwner& owner)
    {
        if (sessionId < 0 || room.FindSlot(sessionId) != owner.slot) return false;

        S2C_Attack placed{};
        if (!PlaceBomb(request, owner, &placed)) return false;
        placed.ownerId = sessionId;
        room.Broadcast(&placed);
        return true;
    }
    /* 수정 - 2026-11-19 */
    template<class Room, class Move>
    void Advance(Room& room, std::uint64_t elapsedMs, Move stepMovement)
    {
        clock.Advance(elapsedMs, stepMovement, [this, &room] {
            StepBomb([&room](const S2C_Explode& packet) { room.Broadcast(&packet); },
                [&room](const S2C_BlockBreak& packet) { room.Broadcast(&packet); });
        });
    }
    /* 수정 - 2026-11-23 */
    template<class Room>
    bool PickUpItem(Room& room, int sessionId, ItemOwner& owner)
    {
        if (sessionId < 0 || room.FindSlot(sessionId) != owner.slot) return false;
        S2C_ItemGet packet{};
        if (!TakeItem(owner, packet)) return false;
        packet.id = sessionId;
        room.Broadcast(&packet);
        return true;
    }
    void StepBomb();
    void StepBomb(const std::function<void(const S2C_Explode&)>& broadcast,
        const std::function<void(const S2C_BlockBreak&)>& blockBreak = {});
    void ResolveExplosion(ServerBomb& bomb,
        const std::function<void(const S2C_BlockBreak&)>& blockBreak = {});
    bool InitBoard(MAP_ID id);
    bool IsWalkable(int row, int col) const
    {
        return row >= 0 && row < MapTiles && col >= 0 && col < MapTiles
            && board[row][col].obstacle == 0;
    }

private:
    bool PlaceBomb(const C2S_Attack& request, const BombOwner& owner, S2C_Attack* placed);
    bool TakeItem(ItemOwner& owner, S2C_ItemGet& packet);
    int nextBombId = 0;
    FixedStepLoop clock;
    std::vector<ServerBomb> bombs;
    Board board[MapTiles][MapTiles]{};
    MAP_ID mapId = MAP_FOREST;
};
