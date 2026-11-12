#pragma once

/* 수정 - 2026-11-12 */
struct ServerBomb {
    enum State { Removed = 0, Fuse = 1, Exploding = 2 };
    static constexpr int FuseTicks = 20;
    static constexpr int FlameTicks = 10;

    int id, ownerSlot, row, col, reach;
    State state = Fuse;
    int ticksLeft = FuseTicks;

    // 폭발을 시작하는 틱에서만 true를 반환한다.
    bool Tick()
    {
        if (state == Removed || --ticksLeft > 0)
            return false;

        if (state == Fuse) {
            state = Exploding;
            ticksLeft = FlameTicks;
            return true;
        }
        state = Removed;
        return false;
    }
};
