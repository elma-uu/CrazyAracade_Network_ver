#pragma once

#include <algorithm>
#include <cstdint>

/* 수정 - 2026-11-14 */
class FixedStepLoop {
public:
    template<class Move, class Bomb>
    void Advance(std::uint64_t elapsedMs, Move stepMovement, Bomb stepBomb)
    {
        while (elapsedMs > 0) {
            unsigned next = std::min(moveLeft, bombLeft);
            unsigned delta = elapsedMs < next ? static_cast<unsigned>(elapsedMs) : next;
            elapsedMs -= delta;
            moveLeft -= delta;
            bombLeft -= delta;
            if (moveLeft == 0) {
                moveLeft = MovementMs;
                stepMovement();
            }
            if (bombLeft == 0) {
                bombLeft = BombMs;
                stepBomb();
            }
        }
    }

private:
    static constexpr unsigned MovementMs = 12;
    static constexpr unsigned BombMs = 50;
    unsigned moveLeft = MovementMs;
    unsigned bombLeft = BombMs;
};
