#pragma once

/* 수정 - 2026-11-12 */
struct ServerBomb {
    enum State { Removed = 0, Fuse = 1, Exploding = 2 };
    static constexpr int FuseTicks = 20;
    static constexpr int FlameTicks = 10;

    int id, ownerSlot, row, col, reach;
    State state = Fuse;
    int ticksLeft = FuseTicks;

    int reachU = 0, reachD = 0, reachL = 0, reachR = 0;

    /* 수정 - 2026-11-25 */
    int ownerId = -1; // 설치 당시 세션 ID: 퇴장 뒤 슬롯이 재사용되어도 유지
    unsigned hitMask = 0;
    bool Covers(int r, int c) const
    {
        if (state != Exploding) return false;
        if (r == row) return c >= col - reachL && c <= col + reachR;
        if (c == col) return r >= row - reachU && r <= row + reachD;
        return false;
    }

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
