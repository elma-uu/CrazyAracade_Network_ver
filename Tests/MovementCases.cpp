// 11/22 이동 동기화 확인 (김동연)
// 서버 PlayerMovement를 12ms씩 돌리고, 나오는 S2C_Move를 클라 Player::UpdateNetMove와
// 같은 공식으로 따라가 봐서 서버·클라가 같은 칸에 도착하는지 본다.
// (Room::StartGame·Room::Update가 아직 없어서 서버+클라 실제 접속 테스트 대신 이걸로 확인)
//
// 빌드 (저장소 루트, x64 Native Tools Command Prompt):
//   cl /nologo /utf-8 /EHsc /std:c++17 /I Common Tests\MovementCases.cpp Server\CrazyArcadeServer\PlayerMovement.cpp Server\GameWorld.cpp
//   MovementCases.exe
#include "../Server/CrazyArcadeServer/PlayerMovement.h"
#include "../Server/GameWorld.h"
#include <cstdio>
#include <cmath>
#include <vector>

static int failed = 0;
static void check(const char* id, bool passed)
{
    printf("%s %s\n", id, passed ? "PASS" : "FAIL");
    if (!passed) failed++;
}

// 테스트용 맵: blocked[row][col] = true면 못 감
static bool blocked[MAP_TILES][MAP_TILES];
static bool CanWalk(int row, int col)
{
    if (row < 0 || row >= MAP_TILES || col < 0 || col >= MAP_TILES) return false;
    return !blocked[row][col];
}
static void ClearMap()
{
    for (int r = 0; r < MAP_TILES; r++)
        for (int c = 0; c < MAP_TILES; c++)
            blocked[r][c] = false;
}

// 클라 Player::SetNetMove / UpdateNetMove와 같은 계산 (Player는 GDI를 써서 여기서 못 씀)
struct ClientPlayer {
    float px = 0, py = 0, destX = 0, destY = 0, speed = 1.0f;
    int dir = IN_DOWN;

    void SetNetMove(const S2C_Move& m)
    {
        px = m.x; py = m.y;
        destX = m.destX; destY = m.destY;
        speed = m.speed;
        dir = m.dir;
    }
    static float Toward(float cur, float target, float step)
    {
        if (cur < target) return (cur + step > target) ? target : cur + step;
        return (cur - step < target) ? target : cur - step;
    }
    void Update(float dt)
    {
        float step = speed * (1000.0f / 96.0f) * dt;
        px = Toward(px, destX, step);
        py = Toward(py, destY, step);
    }
};

int main()
{
    std::vector<S2C_Move> sent;
    auto onMove = [&](const S2C_Move& m) { sent.push_back(m); };

    // MOVE-01 가만히 있으면 아무것도 안 보냄
    {
        ClearMap();
        PlayerMovement mv;
        mv.Spawn(0, 7, 0, 0);
        sent.clear();
        for (int i = 0; i < 20; i++) mv.Step(CanWalk, onMove);
        check("MOVE-01", sent.empty() && mv.Get(0).x == 0 && mv.Get(0).y == 0);
    }

    // MOVE-02 오른쪽을 누르면 첫 스텝에 S2C_Move 하나 (0,0 → 1,0)
    // MOVE-03 속도 1.0이면 8스텝(96ms)에 한 칸 도착
    {
        ClearMap();
        PlayerMovement mv;
        mv.Spawn(0, 7, 0, 0);
        mv.SetInput(0, IN_RIGHT);
        sent.clear();
        mv.Step(CanWalk, onMove);
        bool ok = sent.size() == 1 && sent[0].type == S2C_MOVE && sent[0].size == sizeof(S2C_Move)
            && sent[0].id == 7 && sent[0].x == 0 && sent[0].y == 0
            && sent[0].destX == 1 && sent[0].destY == 0 && sent[0].dir == IN_RIGHT;
        check("MOVE-02", ok);

        for (int i = 0; i < 6; i++) mv.Step(CanWalk, onMove);
        bool notYet = mv.Get(0).x == 0;          // 7스텝째는 아직
        mv.Step(CanWalk, onMove);
        check("MOVE-03", notYet && mv.Get(0).x == 1 && sent.size() == 1);
    }

    // MOVE-04 계속 누르고 있으면 칸마다 한 번씩만 보냄 (5칸 = 5개)
    {
        ClearMap();
        PlayerMovement mv;
        mv.Spawn(0, 7, 0, 0);
        mv.SetInput(0, IN_RIGHT);
        sent.clear();
        for (int i = 0; i < 40; i++) mv.Step(CanWalk, onMove);   // 40스텝 = 5칸
        bool ok = sent.size() == 5 && mv.Get(0).x == 5;
        for (size_t k = 0; k < sent.size(); k++)
            ok = ok && sent[k].x == (int)k && sent[k].destX == (int)k + 1;
        check("MOVE-04", ok);
    }

    // MOVE-05 칸 중간에 손을 떼도 그 칸까지는 가고 멈춤
    {
        ClearMap();
        PlayerMovement mv;
        mv.Spawn(0, 7, 0, 0);
        mv.SetInput(0, IN_DOWN);
        sent.clear();
        for (int i = 0; i < 3; i++) mv.Step(CanWalk, onMove);
        mv.SetInput(0, IN_NONE);
        for (int i = 0; i < 20; i++) mv.Step(CanWalk, onMove);
        check("MOVE-05", mv.Get(0).y == 1 && mv.Get(0).x == 0 && sent.size() == 1);
    }

    // MOVE-06 벽 쪽으로 누르면 못 가고, 방향만 바뀐 걸 한 번만 알림
    {
        ClearMap();
        blocked[0][1] = true;               // (0,0) 오른쪽이 막힘
        PlayerMovement mv;
        mv.Spawn(0, 7, 0, 0);               // 처음 방향은 아래
        mv.SetInput(0, IN_RIGHT);
        sent.clear();
        for (int i = 0; i < 20; i++) mv.Step(CanWalk, onMove);
        bool ok = mv.Get(0).x == 0 && sent.size() == 1
            && sent[0].destX == 0 && sent[0].destY == 0 && sent[0].dir == IN_RIGHT;
        check("MOVE-06", ok);
    }

    // MOVE-07 맵 밖으로는 못 나감 (왼쪽 위 모서리에서 위·왼쪽)
    {
        ClearMap();
        PlayerMovement mv;
        mv.Spawn(0, 7, 0, 0);
        mv.SetInput(0, IN_UP);
        for (int i = 0; i < 20; i++) mv.Step(CanWalk, onMove);
        mv.SetInput(0, IN_LEFT);
        for (int i = 0; i < 20; i++) mv.Step(CanWalk, onMove);
        check("MOVE-07", mv.Get(0).x == 0 && mv.Get(0).y == 0);
    }

    // MOVE-08 이상한 방향 값·빈 자리 입력은 무시
    {
        ClearMap();
        PlayerMovement mv;
        mv.Spawn(0, 7, 0, 0);
        mv.SetInput(0, 9);
        mv.SetInput(2, IN_RIGHT);            // 아무도 없는 자리
        sent.clear();
        for (int i = 0; i < 20; i++) mv.Step(CanWalk, onMove);
        check("MOVE-08", sent.empty() && mv.Get(0).inputDir == IN_NONE && mv.Get(2).sessionId == -1);
    }

    // MOVE-09 진짜 숲 맵(규동 GameWorld)에서 오른쪽으로 계속: 장애물 칸에는 절대 안 들어감
    {
        GameWorld world;
        world.InitBoard(MAP_FOREST);
        auto canWalk = [&](int row, int col) { return world.IsWalkable(row, col); };
        PlayerMovement mv;
        mv.Spawn(0, 7, 0, 0);
        mv.SetInput(0, IN_RIGHT);
        bool ok = true;
        for (int i = 0; i < 400; i++) {
            mv.Step(canWalk, onMove);
            ok = ok && world.IsWalkable(mv.Get(0).y, mv.Get(0).x)
                    && world.IsWalkable(mv.Get(0).destY, mv.Get(0).destX);
        }
        check("MOVE-09", ok);
    }

    // SYNC-01 서버 12ms 스텝 / 클라 16ms 프레임으로 돌려도 클라가 서버와 같은 칸에 도착
    //         (오른쪽 3칸 → 아래 2칸 → 손 뗌)
    // SYNC-02 움직이는 동안 클라와 서버 위치 차이는 한 칸 미만
    {
        ClearMap();
        PlayerMovement mv;
        mv.Spawn(0, 7, 0, 0);
        ClientPlayer cl;
        sent.clear();

        int serverMs = 0;
        bool closeEnough = true;
        for (int clientMs = 0; clientMs <= 1500; clientMs += 16) {
            // 입력: 0~280ms 오른쪽, 300~480ms 아래, 그 뒤 뗌
            unsigned char dir = IN_NONE;
            if (clientMs < 280) dir = IN_RIGHT;
            else if (clientMs >= 300 && clientMs < 480) dir = IN_DOWN;
            mv.SetInput(0, dir);

            while (serverMs + 12 <= clientMs) {        // 서버는 12ms마다
                mv.Step(CanWalk, onMove);
                serverMs += 12;
            }
            for (const S2C_Move& m : sent) cl.SetNetMove(m);   // 클라가 받음 (같은 프레임에 반영)
            sent.clear();
            cl.Update(0.016f);

            float dx = std::fabs(cl.px - mv.Get(0).px), dy = std::fabs(cl.py - mv.Get(0).py);
            closeEnough = closeEnough && dx < 1.0f && dy < 1.0f;
        }
        const ServerPlayer& s = mv.Get(0);
        printf("  server tile (%d,%d)  client (%.2f,%.2f)\n", s.x, s.y, cl.px, cl.py);
        check("SYNC-01", s.x == s.destX && s.y == s.destY && cl.px == (float)s.x && cl.py == (float)s.y);
        check("SYNC-02", closeEnough);
    }

    printf("failed: %d\n", failed);
    return failed;
}
