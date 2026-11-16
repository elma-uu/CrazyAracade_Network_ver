#include "../Server/GameWorld.h"
#include <cstdio>
#include <string>
#include <initializer_list>

int main()
{
    int failed = 0;
    auto check = [&](const char* id, bool passed) {
        std::printf("%s %s\n", id, passed ? "PASS" : "FAIL");
        if (!passed) ++failed;
    };

    GameWorld world;
    for (auto id : { MAP_FOREST, MAP_CITY, MAP_ICE }) {
        S2C_MapInfo packet{};
        bool ok = world.InitBoard(id);
        world.BuildMapInfo(packet);
        ok = ok && packet.size == 804 && packet.type == 17 && packet.mapId == id;
        for (int row = 0; row < 20; ++row)
            for (int col = 0; col < 20; ++col)
                ok = ok && (world.IsWalkable(row, col) == (packet.obstacle[row][col] == 0));
        for (int row : {0, 19})
            for (int col : {0, 19})
                ok = ok && world.IsWalkable(row, col);
        check(id == MAP_FOREST ? "MAP-01" : id == MAP_CITY ? "MAP-02" : "MAP-03", ok);
    }

    ServerBomb bomb{0, 0, 4, 5, 2};
    bool fuse = true;
    for (int i = 1; i < 20; ++i) {
        bool exploded = bomb.Tick();
        fuse = fuse && !exploded && bomb.state == ServerBomb::Fuse;
    }
    check("BOMB-01", fuse);
    check("BOMB-02", bomb.Tick() && bomb.state == ServerBomb::Exploding && bomb.ticksLeft == 10);
    bool flame = true;
    for (int i = 21; i < 30; ++i) {
        bool exploded = bomb.Tick();
        flame = flame && !exploded && bomb.state == ServerBomb::Exploding;
    }
    check("BOMB-03", flame);
    check("BOMB-04", !bomb.Tick() && bomb.state == ServerBomb::Removed && bomb.ticksLeft == 0);
    check("BOMB-05", !bomb.Tick() && bomb.state == ServerBomb::Removed && bomb.ticksLeft == 0);
    world.InitBoard(MAP_FOREST);
    ServerBomb corner{1, 0, 0, 0, 5};
    world.ResolveExplosion(corner);
    check("EXP-01", corner.reachU == 0 && corner.reachL == 0);
    check("EXP-02", corner.reachD == 0 && !world.IsWalkable(1, 0));
    check("EXP-03", corner.reachR == 2 && world.IsWalkable(0, 2)
        && !world.IsWalkable(0, 3));

    ServerBomb shortRange{2, 0, 0, 1, 1};
    world.ResolveExplosion(shortRange);
    check("EXP-04", shortRange.reachL == 1 && shortRange.reachR == 1
        && shortRange.reachD == 1 && !world.IsWalkable(0, 3));
    FixedStepLoop loop;
    std::string events;
    auto move = [&] { events += 'M'; };
    auto tickBomb = [&] { events += 'B'; };
    loop.Advance(11, move, tickBomb);
    check("TIME-01", events.empty());
    loop.Advance(1, move, tickBomb);
    loop.Advance(38, move, tickBomb);
    check("TIME-02", events == "MMMMB");
    loop.Advance(250, move, tickBomb);
    const std::string expected = events;
    check("TIME-03", std::count(events.begin(), events.end(), 'M') == 25
        && std::count(events.begin(), events.end(), 'B') == 6
        && events.substr(events.size() - 2) == "MB");
    events.clear();
    FixedStepLoop single;
    single.Advance(300, move, tickBomb);
    check("TIME-04", events == expected);
    int moves = 0;
    auto countMove = [&] { ++moves; };
    world.Advance(11, countMove);
    world.InitBoard(MAP_CITY);
    world.Advance(1, countMove);
    check("TIME-05", moves == 0);
    world.Advance(11, countMove);
    check("TIME-06", moves == 1);
    world.InitBoard(MAP_CITY);
    C2S_Attack attack{sizeof(C2S_Attack), C2S_ATTACK};
    BombOwner owner{0, 0, 0, true};
    check("ATK-01", world.PlaceBomb(attack, owner) && !world.IsWalkable(0, 0)
        && !world.PlaceBomb(attack, owner));
    owner.col = 1;
    bool second = world.PlaceBomb(attack, owner);
    owner.col = 2;
    check("ATK-02", second && !world.PlaceBomb(attack, owner));
    owner.slot = 1;
    check("ATK-03", world.PlaceBomb(attack, owner));
    owner.slot = 0;
    owner.col = 3;
    world.Advance(1000, [] {});
    check("ATK-04", world.IsWalkable(0, 0) && !world.PlaceBomb(attack, owner));
    world.Advance(500, [] {});
    check("ATK-05", world.PlaceBomb(attack, owner));
    world.InitBoard(MAP_FOREST);
    owner.col = 0;
    owner.apples = 2;
    check("ATK-06", !world.PlaceBomb(attack, owner) && world.IsWalkable(0, 0));
    owner.apples = 0;
    owner.alive = false;
    bool deadRejected = !world.PlaceBomb(attack, owner);
    owner.alive = true;
    attack.size = 0;
    check("ATK-07", deadRejected && !world.PlaceBomb(attack, owner)
        && world.IsWalkable(0, 0));
    return failed;
}
