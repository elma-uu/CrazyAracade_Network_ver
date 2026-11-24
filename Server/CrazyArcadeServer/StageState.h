#pragma once
#include "protocol.h"

// 스테이지 상태 (숲 사과, 사과상자)

struct StagePlayer {
    int  sessionId = -1;   // -1 = 빈 자리
    int  apple = 0;        // 들고 있는 사과
    int  appleBox = 0;     // 내 사과상자에 넣은 사과
    int  heart = 0;        // 얼음
    int  timeLeftMs = 0;   // 도시
    bool alive = false;
};

class StageState {
public:
    // 게임 시작할 때 초기화
    void Init(int mapId);
    void AddPlayer(int slot, int sessionId);

    // 사과나무를 터뜨리면 사과를 준다
    void AddApples(int slot, int count);
    // 내 사과상자 옆이면 사과를 상자에 넣는다
    void DepositApples(int slot, int x, int y);
    // 물풍선에 맞으면 들고 있던 사과를 때린 사람에게 뺏긴다
    void OnHit(int victimSlot, int attackerSlot);

    // 사과상자는 시작 모서리 바로 안쪽 칸
    static void GetAppleBoxTile(int slot, int& x, int& y);

    // 바뀌었을 때만 보낸다
    bool IsChanged() const { return m_changed; }
    void ClearChanged() { m_changed = false; }
    void Fill(S2C_StageState& out) const;

    int GetMapId() const { return m_mapId; }
    const StagePlayer& Get(int slot) const { return m_players[slot]; }

private:
    bool IsValidPlayer(int slot) const;

    int         m_mapId = MAP_FOREST;
    StagePlayer m_players[MAX_ROOM_PLAYER];
    bool        m_changed = false;
};
