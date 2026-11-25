#include "../Server/GameWorld.h"
#include <cstdio>
#include <string>
#include <cstdlib>
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
    struct TestRoom {
        std::vector<S2C_Attack> sends;
        int FindSlot(int id) const { return id == 42 ? 1 : -1; }
        void Broadcast(const void* packet) {
            sends.push_back(*static_cast<const S2C_Attack*>(packet));
        }
    } room;
    world.InitBoard(MAP_CITY);
    attack = {sizeof(C2S_Attack), C2S_ATTACK};
    owner = {1, 0, 1, true, 3};
    bool accepted = world.OnAttack(room, 42, attack, owner);
    check("CAST-01", accepted && room.sends.size() == 1
        && room.sends[0].size == sizeof(S2C_Attack) && room.sends[0].type == S2C_ATTACK
        && room.sends[0].bombId == 0 && room.sends[0].ownerId == 42
        && room.sends[0].x == 1 && room.sends[0].y == 0 && room.sends[0].reach == 3);
    check("CAST-02", !world.OnAttack(room, 42, attack, owner) && room.sends.size() == 1);
    owner.col = 2;
    check("CAST-03", !world.OnAttack(room, 77, attack, owner) && world.IsWalkable(0, 2)
        && room.sends.size() == 1);
    check("CAST-04", world.OnAttack(room, 42, attack, owner) && room.sends.size() == 2
        && room.sends.back().bombId == 1);

    struct ExplosionRoom {
        std::vector<S2C_Explode> sends;
        std::vector<S2C_BlockBreak> blocks;
        void Broadcast(const void* packet) {
            auto header = static_cast<const PACKET_HEADER*>(packet);
            if (header->type == S2C_EXPLODE)
                sends.push_back(*static_cast<const S2C_Explode*>(packet));
            if (header->type == S2C_BLOCK_BREAK)
                blocks.push_back(*static_cast<const S2C_BlockBreak*>(packet));
        }
    } explosionRoom;
    world.InitBoard(MAP_FOREST);
    owner = {0, 0, 0, true, 5};
    world.PlaceBomb(attack, owner);
    world.Advance(explosionRoom, 999, [] {});
    check("EXPCAST-01", explosionRoom.sends.empty());
    world.Advance(explosionRoom, 1, [] {});
    check("EXPCAST-02", explosionRoom.sends.size() == 1
        && explosionRoom.sends[0].size == sizeof(S2C_Explode)
        && explosionRoom.sends[0].type == S2C_EXPLODE
        && explosionRoom.sends[0].bombId == 0
        && explosionRoom.sends[0].reachRight == 2
        && explosionRoom.sends[0].reachDown == 0
        && explosionRoom.sends[0].reachLeft == 0
        && explosionRoom.sends[0].reachUp == 0);
    world.Advance(explosionRoom, 500, [] {});
    check("EXPCAST-03", explosionRoom.sends.size() == 1);
    check("BLOCK-01", explosionRoom.blocks.size() == 1
        && explosionRoom.blocks[0].x == 2 && explosionRoom.blocks[0].y == 0
        && explosionRoom.blocks[0].type == S2C_BLOCK_BREAK);

    struct ItemRoom {
        std::vector<S2C_ItemGet> firstClient, secondClient;
        int FindSlot(int id) const { return id == 42 ? 0 : -1; }
        void Broadcast(const void* packet) {
            firstClient.push_back(*static_cast<const S2C_ItemGet*>(packet));
            secondClient.push_back(*static_cast<const S2C_ItemGet*>(packet));
        }
    } itemRoom;
    std::srand(23);
    world.InitBoard(MAP_FOREST);
    std::vector<S2C_BlockBreak> drops;
    for (int row = 0; row < 20; ++row) {
        for (int col = 0; col < 20; ++col) {
            ServerBomb sweep{0, 0, row, col, 1};
            world.ResolveExplosion(sweep, [&](const S2C_BlockBreak& packet) {
                if (packet.item != ITEM_NONE) drops.push_back(packet);
            });
        }
    }
    ItemOwner collector{};
    collector.slot = 0;
    collector.alive = true;
    bool effects = !drops.empty(), noRepeat = true;
    bool seenPower = false, seenBomb = false, seenSpeed = false;
    for (const auto& drop : drops) {
        collector.row = drop.y;
        collector.col = drop.x;
        collector.speed = 1.5f;
        int oldReach = collector.reach, oldLimit = collector.maxBombs;
        bool picked = world.PickUpItem(itemRoom, 42, collector);
        effects = effects && picked;
        if (drop.item == ITEM_POWER) {
            seenPower = true;
            effects = effects && collector.reach == oldReach + 1;
        }
        if (drop.item == ITEM_BOMB) {
            seenBomb = true;
            effects = effects && collector.maxBombs == oldLimit + 1;
        }
        if (drop.item == ITEM_SPEED) {
            seenSpeed = true;
            effects = effects && collector.speed == 1.5f;
        }
        noRepeat = noRepeat && !world.PickUpItem(itemRoom, 42, collector);
    }
    check("ITEM-01", effects && seenPower && seenBomb && seenSpeed);
    check("ITEM-02", noRepeat && itemRoom.firstClient.size() == drops.size());
    bool same = itemRoom.firstClient.size() == itemRoom.secondClient.size();
    for (size_t i = 0; i < itemRoom.firstClient.size(); ++i) {
        const auto& a = itemRoom.firstClient[i];
        const auto& b = itemRoom.secondClient[i];
        same = same && a.id == 42 && a.type == S2C_ITEM_GET && a.size == sizeof(a)
            && a.x == b.x && a.y == b.y && a.item == b.item && a.speed == b.speed;
    }
    check("ITEM-03", same && !itemRoom.firstClient.empty());
    check("ITEM-04", !world.PickUpItem(itemRoom, 99, collector));
    /* 수정 - 2026-11-25 */
    struct HitRoom {
        std::vector<S2C_Hit> hits;
        int FindSlot(int id) const { return id == 42 ? 0 : -1; }
        void Broadcast(const void* raw) {
            auto* header = static_cast<const PACKET_HEADER*>(raw);
            if (header->type == S2C_HIT) hits.push_back(*static_cast<const S2C_Hit*>(raw));
        }
    } hitRoom;
    world.InitBoard(MAP_FOREST);
    HitPlayer players[MAX_ROOM_PLAYER]{};
    players[0] = {42, 0, 0, true};
    players[1] = {77, 0, 1, true};
    players[2] = {88, 0, 3, true}; // 상자 뒤: 물줄기에 닿지 않음
    players[3] = {99, 0, 0, false};
    C2S_Attack hitRequest{};
    hitRequest.size = sizeof(hitRequest);
    hitRequest.type = C2S_ATTACK;
    BombOwner attacker{0, 0, 0, true};
    attacker.reach = 5;
    check("HIT-01", world.OnAttack(hitRoom, 42, hitRequest, attacker));
    int rulesApplied = 0;
    auto rule = [&](const S2C_Hit&) { ++rulesApplied; };
    world.Advance(hitRoom, 950, [] {}, players, rule);
    check("HIT-02", hitRoom.hits.empty());
    world.Advance(hitRoom, 50, [] {}, players, rule);
    check("HIT-03", hitRoom.hits.size() == 2 && rulesApplied == 2
        && hitRoom.hits[0].victimId == 42 && hitRoom.hits[1].victimId == 77
        && hitRoom.hits[1].attackerId == 42 && hitRoom.hits[1].cause == HIT_BOMB
        && hitRoom.hits[1].size == sizeof(S2C_Hit));
    check("HIT-04", players[0].invincibleTicks == 10
        && players[2].invincibleTicks == 0 && players[3].invincibleTicks == 0);
    world.CheckHits(players, [&](const S2C_Hit& p) { hitRoom.Broadcast(&p); });
    check("HIT-05", hitRoom.hits.size() == 2);
    players[2].col = 1; // 이미 진행 중인 물줄기에 들어와도 피격
    world.Advance(hitRoom, 50, [] {}, players, rule);
    check("HIT-06", hitRoom.hits.size() == 3 && hitRoom.hits.back().victimId == 88);
    world.Advance(hitRoom, 450, [] {}, players, rule);
    check("HIT-07", hitRoom.hits.size() == 3 && players[0].invincibleTicks == 0);
    world.OnAttack(hitRoom, 42, hitRequest, attacker);
    world.Advance(hitRoom, 1000, [] {}, players, rule);
    check("HIT-08", hitRoom.hits.size() == 6); // 다음 폭발에서는 다시 맞음
    ServerBomb range{0, 0, 5, 5, 2};
    range.state = ServerBomb::Exploding;
    range.reachU = range.reachD = range.reachL = range.reachR = 2;
    check("HIT-09", range.Covers(5, 5) && range.Covers(3, 5)
        && range.Covers(7, 5) && range.Covers(5, 3) && range.Covers(5, 7)
        && !range.Covers(4, 4) && !range.Covers(5, 8));
    world.InitBoard(MAP_FOREST);
    hitRoom.hits.clear();
    for (auto& player : players) player = HitPlayer{};
    players[0] = {42, 0, 0, true};
    attacker.col = 0;
    world.OnAttack(hitRoom, 42, hitRequest, attacker);
    attacker.col = 1;
    world.OnAttack(hitRoom, 42, hitRequest, attacker);
    world.Advance(hitRoom, 1000, [] {}, players);
    check("HIT-10", hitRoom.hits.size() == 1); // 두 물줄기가 겹쳐도 무적 시간 중 중복 피해 금지
    players[0].invincibleTicks = 0;
    world.CheckHits(players, [&](const S2C_Hit& p) { hitRoom.Broadcast(&p); });
    check("HIT-11", hitRoom.hits.size() == 2); // 다른 폭발은 무적이 끝나면 판정 가능
    players[0].invincibleTicks = 0;
    world.CheckHits(players, [&](const S2C_Hit& p) { hitRoom.Broadcast(&p); });
    check("HIT-12", hitRoom.hits.size() == 2); // 동일 폭발은 피해자당 한 번
    return failed;
}
