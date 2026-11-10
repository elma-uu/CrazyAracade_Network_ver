#include "Player.h"
#include "TileMap.h"
#include "Input.h"
#include "Gfx.h"
#include "GameData.h"

namespace {
    // 색 선택 칸마다 돌릴 색상(hue) 각도: 빨강, 주황, 노랑, 초록, 민트, 파랑, 보라
    const double kColorHueTable[GameData::ColorCount] = { 0, 30, 60, 120, 150, 210, 270 };
    constexpr int kHitBlinkMs = 500;
}

Player::Player(int index, const KeyBinding& keys, int colorIndex, const TileMap& map)
    : mIndex(index), mKeys(keys), mMap(map)
{
    int color = std::clamp(colorIndex, 0, GameData::ColorCount - 1);
    // 원본 스프라이트의 빨간 계열(0~20도)만 골라 색을 바꾼다
    mSprite = Gfx::CreateHueShiftedCopy(ResourceManager::Get(BitmapID::Player), kColorHueTable[color], 0, 20);
}

Player::~Player()
{
    if (mSprite) DeleteObject(mSprite);
}

void Player::Spawn(int col, int row)
{
    mCol = mDestCol = col;
    mRow = mDestRow = row;
    mPx = static_cast<float>(col);
    mPy = static_cast<float>(row);
    mFacing = Dir::Down;
    mAnimFrame = 0;
    mSpeed = 1.0f;
    mBombRange = 1;
    mMaxBombs = 2;
    mApples = 0;
    mHitTimerMs = 0;
    std::fill(std::begin(mPickedItems), std::end(mPickedItems), 0);
}

int Player::KeyOf(Dir dir) const
{
    switch (dir) {
    case Dir::Up:    return mKeys.up;
    case Dir::Down:  return mKeys.down;
    case Dir::Left:  return mKeys.left;
    case Dir::Right: return mKeys.right;
    default:         return 0;
    }
}

void Player::HandleInput()
{
    // 새로 눌린 방향을 맨 앞으로 → 여러 방향을 누르고 있으면 마지막에 누른 쪽으로 간다
    for (Dir dir : { Dir::Up, Dir::Down, Dir::Left, Dir::Right }) {
        if (!Input::GetKeyDown(KeyOf(dir))) continue;
        auto it = std::find(std::begin(mDirOrder), std::end(mDirOrder), dir);
        std::rotate(std::begin(mDirOrder), it, it + 1);
    }
}

Dir Player::GetHeldDirection() const
{
    for (Dir dir : mDirOrder)
        if (Input::GetKey(KeyOf(dir))) return dir;
    return Dir::None;
}

bool Player::WantsToPlaceBomb() const
{
    return Input::GetKeyDown(mKeys.bomb);
}

void Player::Hit()
{
    if (mHitTimerMs <= 0) mHitTimerMs = kHitBlinkMs;
}

void Player::FixedUpdate()
{
    if (mHitTimerMs > 0) mHitTimerMs -= Config::FixedStepMs;

    // 멈춰 있을 때만 다음 칸을 정한다
    if (!IsMoving()) {
        Dir dir = GetHeldDirection();
        int nextCol = mCol, nextRow = mRow;
        switch (dir) {
        case Dir::Up:    --nextRow; break;
        case Dir::Down:  ++nextRow; break;
        case Dir::Left:  --nextCol; break;
        case Dir::Right: ++nextCol; break;
        default: break;
        }
        if (dir != Dir::None && mMap.IsWalkable(nextRow, nextCol)) {
            mFacing = dir;
            mDestCol = nextCol;
            mDestRow = nextRow;
        }
    }
    if (!IsMoving()) return;

    const float step = mSpeed / 8.0f;
    auto approach = [step](float current, int target) {
        float diff = target - current;
        return fabs(diff) > step ? current + (diff > 0 ? step : -step) : static_cast<float>(target);
    };
    mPx = approach(mPx, mDestCol);
    mPy = approach(mPy, mDestRow);
    if (!IsMoving()) {
        mCol = mDestCol;
        mRow = mDestRow;
    }
    mAnimFrame = (mAnimFrame + 1) % 16;
}

void Player::Render(HDC hdc) const
{
    // 피격 중에는 100ms 간격으로 깜빡임
    if (IsHit() && (GetTickCount64() / 100) % 2 == 1) return;

    int drawX = Config::MapOriginX + static_cast<int>(mPx * Config::TileSize);
    int drawY = Config::MapOriginY + static_cast<int>(mPy * Config::TileSize) - 30;

    int spriteRow = 2;
    switch (mFacing) {
    case Dir::Up:    spriteRow = 1; break;
    case Dir::Down:  spriteRow = 2; break;
    case Dir::Left:  spriteRow = 3; break;
    case Dir::Right: spriteRow = 4; break;
    default: break;
    }
    int srcX = mAnimFrame / 4 * 50;
    int srcY = spriteRow * 60;

    Gfx::Draw(hdc, mSprite, drawX, drawY, Config::TileSize, 60, srcX, srcY, 50, 60);
    Gfx::Draw(hdc, BitmapID::PlayerMask, drawX, drawY, Config::TileSize, 60, srcX, srcY, 50, 60);
}

void Player::RenderPortrait(HDC hdc, int x, int y) const
{
    Gfx::Draw(hdc, mSprite, x, y, 80, 50, 0, 0, 50, 40);
}
