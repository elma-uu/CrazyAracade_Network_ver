#include "Train.h"
#include "Player.h"
#include "TileMap.h"
#include "Gfx.h"

namespace {
    constexpr int kWidth = 1029;
    constexpr int kHeight = 101;
    constexpr int kTop = 320;
    constexpr int kStartLeft = -690;
    constexpr int kEndLeft = 1125;
    constexpr int kDropLeft = 200;      // 이 위치를 지날 때 아이템을 뿌린다
    constexpr int kSpeed = 5;           // 스텝당 px
    constexpr int kIntervalMs = 10000;  // 다음 기차까지 대기 시간
    constexpr int kItemSpeed = 10;      // 날아가는 아이템 스텝당 px
}

Train::Train()
{
    ResetPosition();
}

void Train::ResetPosition()
{
    mRect = { kStartLeft, kTop, kStartLeft + kWidth, kTop + kHeight };
    mHitMask = 0;
}

void Train::FixedUpdate()
{
    mDropItems = false;

    if (!mRunning) {
        mIdleMs += Config::FixedStepMs;
        if (mIdleMs >= kIntervalMs) {
            mIdleMs = 0;
            mRunning = true;
            mHitMask = 0;
        }
        return;
    }

    OffsetRect(&mRect, kSpeed, 0);
    if (mRect.left > kEndLeft) {
        mRunning = false;
        ResetPosition();
        return;
    }
    mDropItems = mRect.left == kDropLeft;
}

bool Train::TryHit(const Player& player, POINT playerPos)
{
    if (!mRunning) return false;
    unsigned bit = 1u << player.GetIndex();
    if (mHitMask & bit) return false;
    if (playerPos.x < mRect.left || playerPos.x > mRect.right || playerPos.y < mRect.top || playerPos.y > mRect.bottom)
        return false;
    mHitMask |= bit;
    return true;
}

POINT Train::GetDropOrigin() const
{
    return { kWidth - 250, mRect.bottom - 40 };
}

void Train::Render(HDC hdc) const
{
    if (!mRunning) return;
    Gfx::Draw(hdc, BitmapID::CityTrain, mRect.left, mRect.top, kWidth, kHeight, 0, 0, kWidth, kHeight);
}

FloatingItem::FloatingItem(ItemType type, POINT from, int targetRow, int targetCol, TileMap& map)
    : mType(type), mPos(from), mTargetRow(targetRow), mTargetCol(targetCol), mMap(map)
{
}

void FloatingItem::FixedUpdate()
{
    POINT target = TileMap::TileCenter(mTargetRow, mTargetCol);
    float dx = static_cast<float>(target.x - mPos.x);
    float dy = static_cast<float>(target.y - mPos.y);
    float distance = sqrtf(dx * dx + dy * dy);

    if (distance < 10.0f) {
        mMap.At(mTargetRow, mTargetCol).item = mType;
        Destroy();
        return;
    }
    mPos.x += static_cast<int>(dx / distance * kItemSpeed);
    mPos.y += static_cast<int>(dy / distance * kItemSpeed);
}

void FloatingItem::Render(HDC hdc) const
{
    if (const Item* item = Item::Get(mType)) item->Render(hdc, mPos.x, mPos.y);
}
