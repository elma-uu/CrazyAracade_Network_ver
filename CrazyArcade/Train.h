#pragma once
#include "GameObject.h"
#include "Item.h"

class Player;
class TileMap;

// 도시 스테이지: 주기적으로 맵을 가로지르며 부딪힌 플레이어의 시간을 깎고 아이템을 뿌린다
class Train : public GameObject {
public:
    Train();

    void FixedUpdate() override;
    void Render(HDC hdc) const override;

    bool IsRunning() const { return mRunning; }
    // 맵 중앙을 지나는 스텝에 한 번 true
    bool ShouldDropItems() const { return mDropItems; }
    // 한 번 지나갈 때 플레이어마다 최대 한 번만 true
    bool TryHit(const Player& player, POINT playerPos);

    // 아이템이 뿌려지는 출발 위치
    POINT GetDropOrigin() const;

private:
    void ResetPosition();

    RECT mRect = {};
    bool mRunning = false;
    bool mDropItems = false;
    int mIdleMs = 0;
    unsigned mHitMask = 0;
};

// 기차에서 떨어져 목표 타일로 날아가는 아이템. 도착하면 타일에 놓이고 사라진다
class FloatingItem : public GameObject {
public:
    FloatingItem(ItemType type, POINT from, int targetRow, int targetCol, TileMap& map);

    void FixedUpdate() override;
    void Render(HDC hdc) const override;

private:
    ItemType mType;
    POINT mPos;
    int mTargetRow, mTargetCol;
    TileMap& mMap;
};
