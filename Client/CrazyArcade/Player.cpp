#include "Player.h"
#include "TileMap.h"
#include "Input.h"
#include "Gfx.h"
#include "GameData.h"

namespace {
    const double kColorHueTable[GameData::ColorCount] = { 0, 30, 60, 120, 150, 210, 270 };
    constexpr int kHitBlinkMs = 500;

    constexpr float kTilesPerSecond = 5.0f;

    constexpr float kBodySize = 1.0f;
    constexpr float kHalfBody = kBodySize / 2.0f;
    constexpr float kEdgeEps = 0.0001f;

    int FirstTile(float lo) { return static_cast<int>(std::floor(lo + kEdgeEps)); }
    int LastTile(float hi)  { return static_cast<int>(std::floor(hi - kEdgeEps)); }

    // ---- 모서리 보정 ----
    // 0.0 = 보정 없음, 0.2 = 8px 이내만, 0.5 = 걸쳐 있으면 거의 항상
    constexpr float kSlideRange = 0.3f;
    // 미끄러지는 빠르기 (이동 속도에 곱하는 값). 1.0 = 걷는 속도와 같음, 0.5 = 절반
    constexpr float kSlideSpeed = 1.0f;

    float MoveToward(float current, float target, float step)
    {
        if (current < target) return std::min(current + step, target);
        return std::max(current - step, target);
    }
}

bool Player::sShowFootDebug = false;

Player::Player(int index, const KeyBinding& keys, int colorIndex, const TileMap& map)
    : mIndex(index), mKeys(keys), mMap(map)
{
    int color = std::clamp(colorIndex, 0, GameData::ColorCount - 1);
    mSprite = Gfx::CreateHueShiftedCopy(ResourceManager::Get(BitmapID::Player), kColorHueTable[color], 0, 20);
}

Player::~Player()
{
    if (mSprite) DeleteObject(mSprite);
}

void Player::Spawn(int col, int row)
{
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

/* 수정 - 2026-11-26 */
void Player::Hit(bool restart)
{
    if (restart || mHitTimerMs <= 0) mHitTimerMs = kHitBlinkMs;
}

// 온라인에서는 이동 입력을 처리하지 않고 시각 효과의 시간만 갱신한다.
void Player::UpdateHitEffect()
{
    mHitTimerMs = std::max(0, mHitTimerMs - Config::FixedStepMs);
}

void Player::FixedUpdate()
{
    UpdateHitEffect();

    Dir dir = GetHeldDirection();
    if (dir == Dir::None) return;
    mFacing = dir;

    const float dt = Config::FixedStepMs / 1000.0f;
    if (Move(dir, mSpeed * kTilesPerSecond * dt))
        mAnimFrame = (mAnimFrame + 1) % 16;
}

bool Player::IsAreaOpen(bool horizontal, int mainLo, int mainHi, int sideLo, int sideHi) const
{
    for (int m = mainLo; m <= mainHi; ++m) {
        for (int s = sideLo; s <= sideHi; ++s) {
            bool open = false;
            if (horizontal) open = mMap.IsWalkable(s, m);
            else            open = mMap.IsWalkable(m, s);
            if (!open) return false;
        }
    }
    return true;
}

bool Player::Move(Dir dir, float distance)
{
    bool horizontal = false;
    int sign = 1;
    switch (dir) {
    case Dir::Left:  horizontal = true;  sign = -1; break;
    case Dir::Right: horizontal = true;  sign = 1;  break;
    case Dir::Up:    horizontal = false; sign = -1; break;
    case Dir::Down:  horizontal = false; sign = 1;  break;
    default: return false;
    }
    float* main = &mPy;
    float* side = &mPx;
    if (horizontal) {
        main = &mPx;
        side = &mPy;
    }
    const float mainBefore = *main;
    const float sideBefore = *side;

    const int mainLo = FirstTile(*main + 0.5f - kHalfBody);
    const int mainHi = LastTile(*main + 0.5f + kHalfBody);
    const int sideLo = FirstTile(*side + 0.5f - kHalfBody);
    const int sideHi = LastTile(*side + 0.5f + kHalfBody);

    int ahead = mainHi + 1;
    if (sign < 0) ahead = mainLo - 1;

    float target = *main + sign * distance;
    if (!IsAreaOpen(horizontal, ahead, ahead, sideLo, sideHi)) {
        if (sign > 0) {
            const float limit = ahead - 0.5f - kHalfBody;
            if (target > limit) target = limit;
        } else {
            const float limit = ahead + 0.5f + kHalfBody;
            if (target < limit) target = limit;
        }
        Slide(horizontal, ahead, *side, distance);
    }
    *main = target;
    return *main != mainBefore || *side != sideBefore;
}

void Player::Slide(bool horizontal, int ahead, float& side, float distance)
{
    const int footSide = FootTile(side);
    const float offset = std::fabs(side - footSide);
    if (offset == 0.0f || offset > kSlideRange) return;

    if (!IsAreaOpen(horizontal, ahead, ahead, footSide, footSide)) return;

    side = MoveToward(side, static_cast<float>(footSide), distance * kSlideSpeed);
}

void Player::Render(HDC hdc) const
{
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

    if (sShowFootDebug) RenderFootDebug(hdc);
}

void Player::RenderFootDebug(HDC hdc) const
{
    RECT tile = { Config::MapOriginX + GetCol() * Config::TileSize,
                  Config::MapOriginY + GetRow() * Config::TileSize, 0, 0 };
    tile.right = tile.left + Config::TileSize;
    tile.bottom = tile.top + Config::TileSize;
    HBRUSH cyan = CreateSolidBrush(RGB(0, 255, 255));
    FrameRect(hdc, &tile, cyan);
    RECT inner = { tile.left + 1, tile.top + 1, tile.right - 1, tile.bottom - 1 };
    FrameRect(hdc, &inner, cyan);
    DeleteObject(cyan);

    const int fx = Config::MapOriginX + static_cast<int>((mPx + 0.5f) * Config::TileSize);
    const int fy = Config::MapOriginY + static_cast<int>((mPy + 0.5f) * Config::TileSize);
    constexpr int r = 4;
    HBRUSH neon = CreateSolidBrush(RGB(57, 255, 20));
    HPEN outline = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HGDIOBJ oldBrush = SelectObject(hdc, neon);
    HGDIOBJ oldPen = SelectObject(hdc, outline);
    Ellipse(hdc, fx - r, fy - r, fx + r + 1, fy + r + 1);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(outline);
    DeleteObject(neon);
}

void Player::RenderPortrait(HDC hdc, int x, int y) const
{
    Gfx::Draw(hdc, mSprite, x, y, 80, 50, 0, 0, 50, 40);
}
