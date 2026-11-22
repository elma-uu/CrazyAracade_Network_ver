#pragma once
#include <functional>
#include "protocol.h"

// 11/18 원본 HandlePlayerMovement를 서버로 옮김 (동연)
// 원본처럼 한 칸 단위로 움직인다.
//  - 멈춰 있을 때 방향키가 눌려 있으면 그 방향 다음 칸을 목적지(dest)로 정한다
//  - 12ms마다 speed/8 칸씩 목적지로 다가가고, 닿으면 그 칸이 현재 칸(x, y)이 된다
//  - 움직이는 도중에는 방향을 바꾸지 않는다 (칸에 도착한 뒤 다음 방향을 본다)
// 방향은 하나만 받으므로 대각선 이동은 없다.

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
    // 게임 시작할 때 자리마다 한 번 (시작 칸은 Room::GetStartTile)
    void Spawn(int slot, int sessionId, int x, int y);

    // 12ms마다 한 번 부른다
    // canWalk(row, col) : 그 칸으로 갈 수 있는지 (GameWorld::IsWalkable)
    void Step(const std::function<bool(int, int)>& canWalk);

    const ServerPlayer& Get(int slot) const { return m_players[slot]; }

private:
    ServerPlayer m_players[MAX_ROOM_PLAYER];
};
