#pragma once
#include "GameObject.h"
#include "Item.h"

class TileMap;

struct KeyBinding {
    int up, down, left, right, bomb;
};

class Player : public GameObject {
public:
    Player(int index, const KeyBinding& keys, int colorIndex, const TileMap& map);
    ~Player() override;
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    void Spawn(int col, int row);

    // 매 프레임: 방향키 우선순위 갱신 (가장 최근에 누른 방향이 우선)
    void HandleInput();
    bool WantsToPlaceBomb() const;

    // 고정 스텝: 피격 깜빡임 타이머, 타일 단위 이동
    void FixedUpdate() override;
    void Render(HDC hdc) const override;
    // 상단 HUD 초상화
    void RenderPortrait(HDC hdc, int x, int y) const;

    void Hit();
    bool IsHit() const { return mHitTimerMs > 0; }

    void OnItemPicked(ItemType type) { ++mPickedItems[static_cast<int>(type)]; }
    int GetPickedCount(ItemType type) const { return mPickedItems[static_cast<int>(type)]; }

    int GetIndex() const { return mIndex; }
    // 현재 서 있는 타일 (이동 중에는 출발 타일, 도착하면 갱신)
    int GetCol() const { return mCol; }
    int GetRow() const { return mRow; }

    float GetSpeed() const { return mSpeed; }
    void SetSpeed(float speed) { mSpeed = speed; }
    int GetBombRange() const { return mBombRange; }
    void SetBombRange(int range) { mBombRange = range; }
    int GetMaxBombs() const { return mMaxBombs; }
    void SetMaxBombs(int count) { mMaxBombs = count; }
    int GetApples() const { return mApples; }
    void SetApples(int apples) { mApples = apples; }

private:
    Dir GetHeldDirection() const;
    int KeyOf(Dir dir) const;
    bool IsMoving() const { return mPx != mDestCol || mPy != mDestRow; }

private:
    const int mIndex;
    const KeyBinding mKeys;
    const TileMap& mMap;
    HBITMAP mSprite = nullptr;  // 선택한 색으로 물들인 전용 스프라이트

    int mCol = 0, mRow = 0;
    int mDestCol = 0, mDestRow = 0;
    float mPx = 0, mPy = 0;     // 타일 단위 실수 좌표 (보간 이동)
    Dir mFacing = Dir::Down;
    Dir mDirOrder[4] = { Dir::Up, Dir::Down, Dir::Left, Dir::Right };
    int mAnimFrame = 0;

    float mSpeed = 1.0f;
    int mBombRange = 1;
    int mMaxBombs = 2;
    int mApples = 0;
    int mHitTimerMs = 0;
    int mPickedItems[static_cast<int>(ItemType::Count)] = {};
};
