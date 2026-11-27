#include "StageState.h"
#include <cstdlib>

namespace {
    constexpr int kIceStartHeart = 3;
}

void StageState::Init(int mapId)
{
    m_mapId = mapId;
    for (int i = 0; i < MAX_ROOM_PLAYER; ++i)
        m_players[i] = StagePlayer();
    m_changed = true;   // 시작 상태를 한 번 보내도록
}

void StageState::AddPlayer(int slot, int sessionId)
{
    if (slot < 0 || slot >= MAX_ROOM_PLAYER) return;

    StagePlayer& p = m_players[slot];
    p = StagePlayer();
    p.sessionId = sessionId;
    p.alive = true;
    if (m_mapId == MAP_ICE) p.heart = kIceStartHeart;
    m_changed = true;
}

bool StageState::IsValidPlayer(int slot) const
{
    if (slot < 0 || slot >= MAX_ROOM_PLAYER) return false;
    if (m_players[slot].sessionId == -1) return false;
    return m_players[slot].alive;
}

void StageState::AddApples(int slot, int count)
{
    if (m_mapId != MAP_FOREST) return;
    if (!IsValidPlayer(slot) || count <= 0) return;

    m_players[slot].apple += count;
    m_changed = true;
}

void StageState::GetAppleBoxTile(int slot, int& x, int& y)
{
    x = 0;
    y = 1;
    if (slot == 1 || slot == 3) x = MAP_TILES - 1;
    if (slot == 2 || slot == 3) y = MAP_TILES - 2;
}

void StageState::DepositApples(int slot, int x, int y)
{
    if (m_mapId != MAP_FOREST) return;
    if (!IsValidPlayer(slot)) return;

    StagePlayer& p = m_players[slot];
    if (p.apple <= 0) return;

    int boxX = 0, boxY = 0;
    GetAppleBoxTile(slot, boxX, boxY);
    int distance = std::abs(x - boxX) + std::abs(y - boxY);
    if (distance != 1) return;   // 상하좌우 바로 옆 칸만

    p.appleBox += p.apple;
    p.apple = 0;
    m_changed = true;
}

void StageState::OnHit(int victimSlot, int attackerSlot)
{
    if (!IsValidPlayer(victimSlot)) return;

    if (m_mapId == MAP_FOREST) OnForestHit(victimSlot, attackerSlot);
    else if (m_mapId == MAP_ICE) OnIceHit(victimSlot);
}

void StageState::OnIceHit(int victimSlot)
{
    StagePlayer& victim = m_players[victimSlot];
    victim.heart--;
    if (victim.heart <= 0) {
        victim.heart = 0;
        victim.alive = false;
    }
    m_changed = true;
}

void StageState::OnForestHit(int victimSlot, int attackerSlot)
{
    StagePlayer& victim = m_players[victimSlot];
    if (victim.apple > 0) {
        // 내 물풍선에 맞았거나 때린 사람이 없으면 뺏어 갈 사람이 없다
        if (victimSlot == attackerSlot) return;
        if (!IsValidPlayer(attackerSlot)) return;

        m_players[attackerSlot].apple += victim.apple;
        victim.apple = 0;
        m_changed = true;
        return;
    }

    if (victim.appleBox > 0) {
        victim.appleBox--;
    } else {
        victim.alive = false;
    }
    m_changed = true;
}

void StageState::Fill(S2C_StageState& out) const
{
    out = S2C_StageState();
    out.size = sizeof(out);
    out.type = S2C_STAGE_STATE;

    for (int i = 0; i < MAX_ROOM_PLAYER; ++i) {
        const StagePlayer& p = m_players[i];
        STAGE_PLAYER& s = out.players[i];
        s.id = p.sessionId;
        if (p.sessionId == -1) continue;   // 빈 자리

        int apple = p.apple;
        if (apple > 255) apple = 255;
        int appleBox = p.appleBox;
        if (appleBox > 255) appleBox = 255;

        s.apple = static_cast<unsigned char>(apple);
        s.appleBox = static_cast<unsigned char>(appleBox);
        s.heart = static_cast<unsigned char>(p.heart);
        s.timeLeft = static_cast<short>(p.timeLeftMs / 1000);   // 초 단위
        s.alive = p.alive;
    }
}
