// 11/23 S2C_StageState 서버 (사과·사과상자) 확인 (김동연)
// Room::StartGame·Room::Update가 아직 없어서, StageState를 서버 이동(PlayerMovement)·숲 맵(GameWorld)과
// 직접 붙여서 확인한다.
//
// 빌드 (저장소 루트, x64 Native Tools Command Prompt):
//   cl /nologo /utf-8 /EHsc /std:c++17 /I Common Tests\StageStateCases.cpp Server\CrazyArcadeServer\StageState.cpp Server\CrazyArcadeServer\PlayerMovement.cpp Server\GameWorld.cpp
//   StageStateCases.exe
#include "../Server/CrazyArcadeServer/StageState.h"
#include "../Server/CrazyArcadeServer/PlayerMovement.h"
#include "../Server/GameWorld.h"
#include <cstdio>

static int failed = 0;
static void check(const char* id, bool passed)
{
    printf("%s %s\n", id, passed ? "PASS" : "FAIL");
    if (!passed) failed++;
}

// 이동 테스트용: 맵 전체가 비어 있음 (막힌 칸 없음)
static bool CanWalkAll(int row, int col)
{
    if (row < 0 || row >= MAP_TILES || col < 0 || col >= MAP_TILES) return false;
    return true;
}
static void IgnoreMove(const S2C_Move&) {}

// 서버 이동을 12ms씩 steps번 돌리면서, 매번 Room::StepMovement처럼 사과상자 넣기를 검사한다
static void Run(PlayerMovement& move, StageState& stage, int steps)
{
    for (int i = 0; i < steps; ++i) {
        move.Step(CanWalkAll, IgnoreMove);
        for (int slot = 0; slot < MAX_ROOM_PLAYER; ++slot) {
            const ServerPlayer& p = move.Get(slot);
            if (p.sessionId == -1) continue;
            stage.DepositApples(slot, p.x, p.y);
        }
    }
}

int main()
{
    // S1 게임 시작 상태
    StageState stage;
    stage.Init(MAP_FOREST);
    stage.AddPlayer(0, 10);
    stage.AddPlayer(1, 11);
    check("S1 start: apple 0, box 0, alive, changed",
          stage.Get(0).apple == 0 && stage.Get(0).appleBox == 0 && stage.Get(0).alive &&
          stage.Get(2).sessionId == -1 && stage.IsChanged());

    // S2 사과상자 칸이 GameWorld 숲 맵의 사과상자(장애물 3)와 같은 칸인지
    GameWorld world;
    world.InitBoard(MAP_FOREST);
    S2C_MapInfo map{};
    world.BuildMapInfo(map);
    bool allBoxes = true;
    for (int slot = 0; slot < MAX_ROOM_PLAYER; ++slot) {
        int x = 0, y = 0;
        StageState::GetAppleBoxTile(slot, x, y);
        if (map.obstacle[y][x] != 3) allBoxes = false;
    }
    check("S2 box tiles match GameWorld forest boxes (0,1)(19,1)(0,18)(19,18)", allBoxes);

    // S3 사과 받기: 숲에서만
    stage.ClearChanged();
    stage.AddApples(0, 2);
    check("S3 forest: AddApples -> apple 2, changed", stage.Get(0).apple == 2 && stage.IsChanged());
    StageState ice;
    ice.Init(MAP_ICE);
    ice.AddPlayer(0, 10);
    ice.AddApples(0, 2);
    check("S4 ice: AddApples ignored", ice.Get(0).apple == 0);

    // S5 상자 옆이 아니면 안 넣음 (자리 0 상자 = (0,1))
    stage.ClearChanged();
    stage.DepositApples(0, 2, 1);
    check("S5 not next to box (2,1): keep apples", stage.Get(0).apple == 2 && stage.Get(0).appleBox == 0 && !stage.IsChanged());

    // S6 남의 상자 옆이면 안 넣음 (자리 1 상자 = (19,1))
    stage.DepositApples(0, 18, 1);
    check("S6 next to other's box: keep apples", stage.Get(0).apple == 2 && stage.Get(0).appleBox == 0);

    // S7 내 상자 바로 옆이면 넣음
    stage.DepositApples(0, 1, 1);
    check("S7 next to my box (1,1): box 2, apple 0, changed",
          stage.Get(0).apple == 0 && stage.Get(0).appleBox == 2 && stage.IsChanged());

    // S8 들고 있는 사과가 없으면 바뀐 것 없음
    stage.ClearChanged();
    stage.DepositApples(0, 1, 1);
    check("S8 no apples: nothing changes", stage.Get(0).appleBox == 2 && !stage.IsChanged());

    // S9 패킷 내용
    stage.AddApples(1, 3);
    S2C_StageState pkt;
    stage.Fill(pkt);
    check("S9 packet size 43 / type", pkt.size == sizeof(S2C_StageState) && pkt.size == 43 && pkt.type == S2C_STAGE_STATE);
    check("S10 packet values", pkt.players[0].id == 10 && pkt.players[0].appleBox == 2 && pkt.players[0].apple == 0 &&
          pkt.players[1].id == 11 && pkt.players[1].apple == 3 && pkt.players[1].alive &&
          pkt.players[2].id == -1 && pkt.players[3].id == -1);

    // S11 서버 이동과 함께: (0,0)에서 오른쪽 3칸 → 사과 2개 받음 → 왼쪽으로 돌아오면 상자 옆 (0,0)에서 넣음
    PlayerMovement move;
    StageState s2;
    s2.Init(MAP_FOREST);
    move.Spawn(0, 10, 0, 0);
    s2.AddPlayer(0, 10);
    move.SetInput(0, IN_RIGHT);
    Run(move, s2, 8 * 3);   // 한 칸 96ms = 12ms × 8
    move.SetInput(0, IN_NONE);
    Run(move, s2, 8);
    s2.AddApples(0, 2);
    bool carrying = (move.Get(0).x == 3 && s2.Get(0).apple == 2 && s2.Get(0).appleBox == 0);
    move.SetInput(0, IN_LEFT);
    Run(move, s2, 8 * 3);
    move.SetInput(0, IN_NONE);
    Run(move, s2, 8);
    check("S11 walk away with apples, come back next to box -> deposited",
          carrying && move.Get(0).x == 0 && s2.Get(0).apple == 0 && s2.Get(0).appleBox == 2);

    printf(failed == 0 ? "ALL PASS\n" : "FAILED: %d\n", failed);
    return failed;
}
