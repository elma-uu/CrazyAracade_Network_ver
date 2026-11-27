// 사과·사과상자 서버 처리 확인
// 빌드:
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

// 막힌 칸 없는 맵
static bool CanWalkAll(int row, int col)
{
    if (row < 0 || row >= MAP_TILES || col < 0 || col >= MAP_TILES) return false;
    return true;
}
static void IgnoreMove(const S2C_Move&) {}

// Room::StepMovement처럼 이동하고 사과상자 검사
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
    // 게임 시작 상태
    StageState stage;
    stage.Init(MAP_FOREST);
    stage.AddPlayer(0, 10);
    stage.AddPlayer(1, 11);
    check("start: apple 0, box 0, alive, changed",
          stage.Get(0).apple == 0 && stage.Get(0).appleBox == 0 && stage.Get(0).alive &&
          stage.Get(2).sessionId == -1 && stage.IsChanged());

    // 사과상자 칸이 GameWorld 숲 맵의 사과상자(장애물 3)와 같은 칸인지
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
    check("box tiles match GameWorld forest boxes (0,1)(19,1)(0,18)(19,18)", allBoxes);

    // 사과 받기: 숲에서만
    stage.ClearChanged();
    stage.AddApples(0, 2);
    check("forest: AddApples -> apple 2, changed", stage.Get(0).apple == 2 && stage.IsChanged());
    StageState ice;
    ice.Init(MAP_ICE);
    ice.AddPlayer(0, 10);
    ice.AddApples(0, 2);
    check("ice: AddApples ignored", ice.Get(0).apple == 0);

    // 상자 옆이 아니면 안 넣음
    stage.ClearChanged();
    stage.DepositApples(0, 2, 1);
    check("not next to box (2,1): keep apples", stage.Get(0).apple == 2 && stage.Get(0).appleBox == 0 && !stage.IsChanged());

    // 남의 상자 옆이면 안 넣음
    stage.DepositApples(0, 18, 1);
    check("next to other's box: keep apples", stage.Get(0).apple == 2 && stage.Get(0).appleBox == 0);

    // 내 상자 바로 옆이면 넣음
    stage.DepositApples(0, 1, 1);
    check("next to my box (1,1): box 2, apple 0, changed",
          stage.Get(0).apple == 0 && stage.Get(0).appleBox == 2 && stage.IsChanged());

    // 들고 있는 사과가 없으면 바뀐 것 없음
    stage.ClearChanged();
    stage.DepositApples(0, 1, 1);
    check("no apples: nothing changes", stage.Get(0).appleBox == 2 && !stage.IsChanged());

    // 패킷 내용
    stage.AddApples(1, 3);
    S2C_StageState pkt;
    stage.Fill(pkt);
    check("packet size 43 / type", pkt.size == sizeof(S2C_StageState) && pkt.size == 43 && pkt.type == S2C_STAGE_STATE);
    check("packet values", pkt.players[0].id == 10 && pkt.players[0].appleBox == 2 && pkt.players[0].apple == 0 &&
          pkt.players[1].id == 11 && pkt.players[1].apple == 3 && pkt.players[1].alive &&
          pkt.players[2].id == -1 && pkt.players[3].id == -1);

    // 사과를 들고 멀리 갔다가 상자 옆으로 돌아오면 넣어진다
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
    check("walk away with apples, come back next to box -> deposited",
          carrying && move.Get(0).x == 0 && s2.Get(0).apple == 0 && s2.Get(0).appleBox == 2);

    // 11/25 맞으면 사과 뺏기
    StageState h;
    h.Init(MAP_FOREST);
    h.AddPlayer(0, 10);
    h.AddPlayer(1, 11);
    h.AddPlayer(2, 12);
    h.AddApples(0, 3);
    h.AddApples(1, 1);
    h.ClearChanged();

    // 1번이 0번을 맞히면 0번 사과 3개가 1번에게
    h.OnHit(0, 1);
    check("hit -> attacker takes victim's apples",
          h.Get(0).apple == 0 && h.Get(1).apple == 4 && h.IsChanged());

    // 사과도 상자도 비었으면 탈락
    h.ClearChanged();
    h.OnHit(0, 2);
    check("hit with no apples and empty box -> eliminated",
          !h.Get(0).alive && h.Get(2).apple == 0 && h.IsChanged());

    // 탈락한 사람은 더 맞아도 그대로
    h.ClearChanged();
    h.OnHit(0, 2);
    check("eliminated player ignores hits", !h.Get(0).alive && !h.IsChanged());

    // 내 물풍선에 맞으면 뺏어 갈 사람 없음
    h.OnHit(1, 1);
    check("own bomb -> keeps apples", h.Get(1).apple == 4 && !h.IsChanged());

    // 빈 자리·이상한 번호가 때린 걸로 오면 무시
    h.OnHit(1, 3);
    h.OnHit(1, -1);
    h.OnHit(1, 9);
    check("invalid attacker -> ignored", h.Get(1).apple == 4 && !h.IsChanged());

    // 11/26 사과가 없으면 상자에서 하나 잃는다 (때린 사람이 가져가지는 않음)
    h.AddApples(2, 2);
    h.DepositApples(2, 1, 18);   // 2번 상자 (0,18) 옆
    h.ClearChanged();
    h.OnHit(2, 1);
    check("no apples -> lose one from box",
          h.Get(2).appleBox == 1 && h.Get(2).alive && h.Get(1).apple == 4 && h.IsChanged());

    // 내 물풍선이어도 상자에서 잃는다
    h.OnHit(2, 2);
    check("own bomb with no apples -> lose one from box", h.Get(2).appleBox == 0 && h.Get(2).alive);

    // 상자까지 비었을 때 맞으면 탈락
    h.OnHit(2, 1);
    check("empty box -> eliminated", !h.Get(2).alive);

    // 탈락하면 사과를 받거나 넣지 않는다
    h.ClearChanged();
    h.AddApples(2, 1);
    h.DepositApples(2, 1, 18);
    check("eliminated player gets no apples", h.Get(2).apple == 0 && !h.IsChanged());

    // 탈락하면 서버 이동도 멈춘다 (Room::OnPlayerHit과 같은 순서)
    PlayerMovement em;
    StageState es;
    es.Init(MAP_FOREST);
    em.Spawn(0, 10, 0, 0);
    es.AddPlayer(0, 10);
    em.SetInput(0, IN_RIGHT);
    es.OnHit(0, -1);
    if (!es.IsAlive(0)) em.Eliminate(0);
    Run(em, es, 8 * 3);
    check("eliminated -> stops moving", em.Get(0).x == 0 && !em.Get(0).alive);

    // 11/27 얼음: 하트 3개로 시작, 맞을 때마다 하나씩, 다 잃으면 탈락
    StageState ice2;
    ice2.Init(MAP_ICE);
    ice2.AddPlayer(0, 10);
    ice2.AddPlayer(1, 11);
    check("ice start heart 3", ice2.Get(0).heart == 3 && ice2.Get(1).heart == 3);

    ice2.ClearChanged();
    ice2.OnHit(0, 1);
    check("ice hit -> heart 2, no apples", ice2.Get(0).heart == 2 && ice2.Get(1).apple == 0 && ice2.IsChanged());

    ice2.OnHit(0, 0);
    check("ice own bomb also counts -> heart 1", ice2.Get(0).heart == 1 && ice2.Get(0).alive);

    ice2.OnHit(0, 1);
    check("ice heart 0 -> eliminated", ice2.Get(0).heart == 0 && !ice2.Get(0).alive);

    ice2.ClearChanged();
    ice2.OnHit(0, 1);
    check("ice eliminated ignores hits", ice2.Get(0).heart == 0 && !ice2.IsChanged());

    S2C_StageState icePkt;
    ice2.Fill(icePkt);
    check("ice packet hearts / alive",
          icePkt.players[0].heart == 0 && !icePkt.players[0].alive &&
          icePkt.players[1].heart == 3 && icePkt.players[1].alive);

    // 숲 상자 규칙은 얼음에 안 섞인다
    check("ice has no apple box rule", ice2.Get(1).appleBox == 0 && ice2.Get(1).alive);

    // 도시는 아직 맞아도 상태 변화 없음 (타이머는 11/30)
    StageState city;
    city.Init(MAP_CITY);
    city.AddPlayer(0, 10);
    city.ClearChanged();
    city.OnHit(0, -1);
    check("city hit -> no stage change yet", city.Get(0).alive && city.Get(0).heart == 0 && !city.IsChanged());

    printf(failed == 0 ? "ALL PASS\n" : "FAILED: %d\n", failed);
    return failed;
}
