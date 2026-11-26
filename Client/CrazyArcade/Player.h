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

    void HandleInput();
    bool WantsToPlaceBomb() const;

    void FixedUpdate() override;
    void Render(HDC hdc) const override;
    void RenderPortrait(HDC hdc, int x, int y) const;

    /* 수정 - 2026-11-26 */
    void Hit(bool restart = false);
    void UpdateHitEffect();
    bool IsHit() const { return mHitTimerMs > 0; }

    void OnItemPicked(ItemType type) { ++mPickedItems[static_cast<int>(type)]; }
    int GetPickedCount(ItemType type) const { return mPickedItems[static_cast<int>(type)]; }

    int GetIndex() const { return mIndex; }
    int GetCol() const { return FootTile(mPx); }
    int GetRow() const { return FootTile(mPy); }

    float GetSpeed() const { return mSpeed; }
    void SetSpeed(float speed) { mSpeed = speed; }
    int GetBombRange() const { return mBombRange; }
    void SetBombRange(int range) { mBombRange = range; }
    int GetMaxBombs() const { return mMaxBombs; }
    void SetMaxBombs(int count) { mMaxBombs = count; }
    int GetApples() const { return mApples; }
    void SetApples(int apples) { mApples = apples; }

    static void ToggleFootDebug() { sShowFootDebug = !sShowFootDebug; }

private:
    void RenderFootDebug(HDC hdc) const;
    static bool sShowFootDebug;

    Dir GetHeldDirection() const;
    int KeyOf(Dir dir) const;
    bool Move(Dir dir, float distance);
    bool IsAreaOpen(bool horizontal, int mainLo, int mainHi, int sideLo, int sideHi) const;
    void Slide(bool horizontal, int ahead, float& side, float distance);
    static int FootTile(float p) { return static_cast<int>(std::floor(p + 0.5f)); }

private:
    const int mIndex;
    const KeyBinding mKeys;
    const TileMap& mMap;
    HBITMAP mSprite = nullptr;

    float mPx = 0, mPy = 0;
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
