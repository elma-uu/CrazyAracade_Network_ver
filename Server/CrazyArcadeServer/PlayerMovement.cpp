#include "PlayerMovement.h"

// 원본 이동 속도: 12ms마다 speed / 8 칸 → 속도 1.0이면 한 칸에 96ms
static const float MOVE_PER_STEP = 1.0f / 8.0f;

void PlayerMovement::Clear()
{
    for (int i = 0; i < MAX_ROOM_PLAYER; i++)
        m_players[i] = ServerPlayer();
}

void PlayerMovement::Spawn(int slot, int sessionId, int x, int y)
{
    if (slot < 0 || slot >= MAX_ROOM_PLAYER) return;

    ServerPlayer& p = m_players[slot];
    p = ServerPlayer();
    p.sessionId = sessionId;
    p.x = p.destX = x;
    p.y = p.destY = y;
    p.px = (float)x;
    p.py = (float)y;
    p.alive = true;
}

void PlayerMovement::SetInput(int slot, unsigned char dir)
{
    if (slot < 0 || slot >= MAX_ROOM_PLAYER) return;
    if (dir > IN_RIGHT) return;   // 이상한 값은 무시
    if (m_players[slot].sessionId == -1) return;

    m_players[slot].inputDir = dir;
}

// target 쪽으로 step만큼 (넘어가지 않게)
static float Approach(float cur, float target, float step)
{
    if (cur < target) {
        cur += step;
        if (cur > target) cur = target;
    }
    else if (cur > target) {
        cur -= step;
        if (cur < target) cur = target;
    }
    return cur;
}

// 11/20 S2C_Move 만들기
void PlayerMovement::MakeMovePacket(int slot, S2C_Move& out) const
{
    const ServerPlayer& p = m_players[slot];
    out = S2C_Move();
    out.size = sizeof(out);
    out.type = S2C_MOVE;
    out.id = p.sessionId;
    out.x = (unsigned char)p.x;
    out.y = (unsigned char)p.y;
    out.destX = (unsigned char)p.destX;
    out.destY = (unsigned char)p.destY;
    out.dir = p.dir;
    out.speed = p.speed;
}

void PlayerMovement::Step(const std::function<bool(int, int)>& canWalk,
                          const std::function<void(const S2C_Move&)>& onMove)
{
    for (int i = 0; i < MAX_ROOM_PLAYER; i++) {
        ServerPlayer& p = m_players[i];
        if (p.sessionId == -1 || !p.alive) continue;

        bool moving = (p.x != p.destX || p.y != p.destY);

        // 멈춰 있을 때만 다음 칸을 정한다 (원본 HandlePlayerMovement와 같음)
        if (!moving) {
            if (p.inputDir == IN_NONE) continue;

            bool turned = (p.dir != p.inputDir);
            p.dir = p.inputDir;

            int nx = p.x, ny = p.y;
            if (p.inputDir == IN_UP) ny--;
            else if (p.inputDir == IN_DOWN) ny++;
            else if (p.inputDir == IN_LEFT) nx--;
            else if (p.inputDir == IN_RIGHT) nx++;

            if (!canWalk(ny, nx)) {
                // 막힌 칸 → 방향만 바뀜. 다른 사람 화면에서도 돌아보게 한 번만 알려 준다 (dest = 지금 칸)
                if (turned) {
                    S2C_Move m;
                    MakeMovePacket(i, m);
                    onMove(m);
                }
                continue;
            }

            p.destX = nx;
            p.destY = ny;

            // 새 칸으로 출발할 때만 보낸다 (12ms마다 좌표를 보내지 않음)
            // 클라는 x,y → destX,destY를 speed로 알아서 보간한다
            S2C_Move m;
            MakeMovePacket(i, m);
            onMove(m);
        }

        // 목적 칸으로 다가가기
        float step = p.speed * MOVE_PER_STEP;
        p.px = Approach(p.px, (float)p.destX, step);
        p.py = Approach(p.py, (float)p.destY, step);

        // 도착
        if (p.px == (float)p.destX && p.py == (float)p.destY) {
            p.x = p.destX;
            p.y = p.destY;
        }
    }
}
