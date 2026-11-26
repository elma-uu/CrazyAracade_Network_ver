// 수정 - 2026-11-26: 실제 Player의 시각 효과 타이머 검증 (창/사운드 초기화 없음)
#include "../Client/CrazyArcade/Player.h"
#include "../Client/CrazyArcade/TileMap.h"
#include <cstdio>

int main()
{
    TileMap map;
    Player player(0, KeyBinding{}, 0, map);
    player.Spawn(3, 4);
    int failed = 0;
    auto check = [&](const char* name, bool ok) {
        std::printf("%s %s\n", name, ok ? "PASS" : "FAIL");
        if (!ok) ++failed;
    };
    check("CLIENT-HIT-01", !player.IsHit());
    player.Hit(true);
    for (int i = 0; i < 41; ++i) player.UpdateHitEffect(); // 492ms
    check("CLIENT-HIT-02", player.IsHit());
    player.UpdateHitEffect(); // 504ms: 12ms 틱 경계에서 종료
    check("CLIENT-HIT-03", !player.IsHit());
    player.Hit(true);
    for (int i = 0; i < 40; ++i) player.UpdateHitEffect();
    player.Hit(true); // 다음 서버 피격은 효과 시간을 다시 시작
    for (int i = 0; i < 40; ++i) player.UpdateHitEffect();
    check("CLIENT-HIT-04", player.IsHit());
    player.UpdateHitEffect();
    player.UpdateHitEffect();
    check("CLIENT-HIT-05", !player.IsHit() && player.GetCol() == 3 && player.GetRow() == 4);
    player.Hit();
    for (int i = 0; i < 41; ++i) player.UpdateHitEffect();
    player.Hit(); // 기존 로컬 호출은 남아 있는 시간을 연장하지 않음
    player.UpdateHitEffect();
    check("CLIENT-HIT-06", !player.IsHit());
    return failed;
}
