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

    // 고정 스텝: 피격 깜빡임 타이머, 실수 좌표 이동
    void FixedUpdate() override;
    void Render(HDC hdc) const override;
    // 상단 HUD 초상화
    void RenderPortrait(HDC hdc, int x, int y) const;

    void Hit();
    bool IsHit() const { return mHitTimerMs > 0; }

    void OnItemPicked(ItemType type) { ++mPickedItems[static_cast<int>(type)]; }
    int GetPickedCount(ItemType type) const { return mPickedItems[static_cast<int>(type)]; }

    int GetIndex() const { return mIndex; }
    // 판정 칸 = 발밑 중앙 한 점이 들어 있는 칸.
    // 칸 사이에 걸쳐 서 있어도(걸치기) 물풍선·피격·아이템 판정은 이 한 칸으로만 한다
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

    // [디버그] 발밑 중심점(형광 초록 점)과 지금 판정 칸(하늘색 테두리) 표시, F3으로 켜고 끔
    static void ToggleFootDebug() { sShowFootDebug = !sShowFootDebug; }

private:
    void RenderFootDebug(HDC hdc) const;
    static bool sShowFootDebug;

    Dir GetHeldDirection() const;
    int KeyOf(Dir dir) const;
    // dir 방향으로 distance(칸 단위)만큼 이동. 실제로 움직였으면 true
    bool Move(Dir dir, float distance);
    // horizontal이면 (mainTile = 열, sideTile = 행), 아니면 (mainTile = 행, sideTile = 열)인 칸이 지나갈 수 있는지
    bool IsOpen(bool horizontal, int mainTile, int sideTile) const;
    // 실수 좌표 → 발밑 중앙 점의 칸 (1.7 → 2, 1.4 → 1)
    static int FootTile(float p) { return static_cast<int>(std::floor(p + 0.5f)); }

private:
    const int mIndex;
    const KeyBinding mKeys;
    const TileMap& mMap;
    HBITMAP mSprite = nullptr;  // 선택한 색으로 물들인 전용 스프라이트

    float mPx = 0, mPy = 0;     // 타일 단위 실수 좌표. 칸 번호는 FootTile로 계산
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
