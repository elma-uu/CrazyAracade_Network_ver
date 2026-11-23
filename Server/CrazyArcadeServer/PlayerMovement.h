#pragma once
#include <functional>
#include "protocol.h"

// 서버 쪽 플레이어 이동 (원본처럼 한 칸 단위)

struct ServerPlayer {
    int   sessionId = -1;            // -1 = 빈 자리
    int   x = 0, y = 0;              // 현재 칸 (x = 열, y = 행)
    int   destX = 0, destY = 0;      // 가는 중인 칸 (멈춰 있으면 x, y와 같음)
    float px = 0, py = 0;            // 보간 좌표 (칸 단위)
    float speed = 1.0f;              // 1.0 ~ 1.5 (아이템)
    unsigned char dir = IN_DOWN;     // 바라보는 방향
    unsigned char inputDir = IN_NONE;   // 클라가 누르고 있는 방향
    bool  alive = false;
};

class PlayerMovement {
public:
    void Clear();
    // 게임 시작할 때 자리마다 부른다
    void Spawn(int slot, int sessionId, int x, int y);

    // 누르고 있는 방향, IN_NONE이면 뗌
    void SetInput(int slot, unsigned char dir);

    // 12ms마다 부른다
    void Step(const std::function<bool(int, int)>& canWalk,
              const std::function<void(const S2C_Move&)>& onMove);

    const ServerPlayer& Get(int slot) const { return m_players[slot]; }

private:
    void MakeMovePacket(int slot, S2C_Move& out) const;

    ServerPlayer m_players[MAX_ROOM_PLAYER];
};
