#pragma once
#include "protocol.h"

// 11/23 스테이지 상태 (동연): 숲 사과·사과상자
// 피격 시 사과 뺏기는 11/25, 사과상자 감소·탈락은 11/26, 얼음 하트는 11/27, 도시 타이머는 11/30

struct StagePlayer {
    int  sessionId = -1;   // -1 = 빈 자리
    int  apple = 0;        // 들고 있는 사과
    int  appleBox = 0;     // 내 사과상자에 넣은 사과
    int  heart = 0;        // 얼음 (11/27)
    int  timeLeftMs = 0;   // 도시 (11/30)
    bool alive = false;
};

class StageState {
public:
    // 게임 시작할 때 맵을 정하고 참가자를 모두 지운다
    void Init(int mapId);
    void AddPlayer(int slot, int sessionId);

    // 숲: 사과나무를 터뜨린 사람에게 사과를 준다 (물풍선 폭발과 연결할 때 부른다)
    void AddApples(int slot, int count);
    // 숲: 내 사과상자 옆 칸(상하좌우)에 서 있으면 들고 있던 사과를 모두 상자에 넣는다
    void DepositApples(int slot, int x, int y);

    // 숲 사과상자 칸 = 시작 모서리 바로 안쪽 칸 (GameWorld::InitBoard와 같은 위치)
    // 자리 0 (0,1), 1 (19,1), 2 (0,18), 3 (19,18). x = 열, y = 행
    static void GetAppleBoxTile(int slot, int& x, int& y);

    // 값이 바뀌었는지 (바뀌었을 때만 S2C_StageState를 보낸다)
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
