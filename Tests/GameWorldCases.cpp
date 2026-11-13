#include "../Server/GameWorld.h"
#include <cstdio>
#include <initializer_list>

int main()
{
    int failed = 0;
    auto check = [&](const char* id, bool passed) {
        std::printf("%s %s\n", id, passed ? "PASS" : "FAIL");
        if (!passed) ++failed;
    };

    GameWorld world;
    for (auto id : { MapId::Forest, MapId::City, MapId::Ice }) {
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
        check(id == MapId::Forest ? "MAP-01" : id == MapId::City ? "MAP-02" : "MAP-03", ok);
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
    world.InitBoard(MapId::Forest);
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
    return failed;
}
