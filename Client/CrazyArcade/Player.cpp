#include "Player.h"
#include "TileMap.h"
#include "Input.h"
#include "Gfx.h"
#include "GameData.h"
#include "../../Common/protocol.h"

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

    // bazzi_die.bmp는 70x70 칸, overlayCol < 0이면 겹칠 그림 없음
    struct DieFrame { int col, row, overlayCol, overlayRow, ms; };
    const DieFrame kDieFrames[] = {
        { 0, 0, -1, 0, 120 }, { 1, 0, -1, 0, 120 }, { 2, 0, -1, 0, 120 }, { 1, 0, -1, 0, 120 },
        { 0, 0, -1, 0, 120 }, { 1, 0, -1, 0, 120 }, { 2, 0, -1, 0, 120 },
        { 0, 1, -1, 0, 90 },
        { 0, 2, -1, 0, 90 },
        { 1, 2, 2, 2, 140 },
        { 1, 1, 2, 1, 140 },
        { 3, 2, -1, 0, 160 },
        { 4, 2, -1, 0, 0 },
    };
    constexpr int kDieFrameCount = sizeof(kDieFrames) / sizeof(kDieFrames[0]);
    constexpr int kDieCell = 70;
    constexpr int kDieDrawW = 56;   // 걷는 그림과 같은 가로 비율 (50 → 40)
    constexpr int kDieDrawH = 70;
    constexpr int kDieFeetY = 64;   // 칸 안에서 발이 닿는 높이
    constexpr int kBubbleSize = 62; // 물방울은 동그랗게
    constexpr int kBubbleFeetY = 57;
    constexpr int kShadowCol = 5, kShadowRow = 2;
}

bool Player::sShowFootDebug = false;

Player::Player(int index, const KeyBinding& keys, int colorIndex, const TileMap& map)
    : mIndex(index), mKeys(keys), mMap(map)
{
    int color = std::clamp(colorIndex, 0, GameData::ColorCount - 1);
    mSprite = Gfx::CreateHueShiftedCopy(ResourceManager::Get(BitmapID::Player), kColorHueTable[color], 0, 20);
    mDieSprite = Gfx::CreateHueShiftedCopy(ResourceManager::Get(BitmapID::PlayerDie), kColorHueTable[color], 0, 20);
}

Player::~Player()
{
    if (mSprite) DeleteObject(mSprite);
    if (mDieSprite) DeleteObject(mDieSprite);
}

void Player::Spawn(int col, int row)
{
    mPx = static_cast<float>(col);
    mPy = static_cast<float>(row);
    mNetDestX = mPx;
    mNetDestY = mPy;
    mFacing = Dir::Down;
    mAnimFrame = 0;
    mSpeed = 1.0f;
    mBombRange = 1;
    mMaxBombs = 2;
    mApples = 0;
    mHitTimerMs = 0;
    mDead = false;
    mDieMs = 0;
    std::fill(std::begin(mPickedItems), std::end(mPickedItems), 0);
}

void Player::Die()
{
    if (mDead) return;
    mDead = true;
    mDieMs = 0;
    mHitTimerMs = 0;
    mNetDestX = mPx;
    mNetDestY = mPy;
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

void Player::Hit()
{
    if (mHitTimerMs <= 0) mHitTimerMs = kHitBlinkMs;
}

void Player::FixedUpdate()
{
    if (mHitTimerMs > 0) mHitTimerMs -= Config::FixedStepMs;

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

// 서버가 알려 준 이동
void Player::SetNetMove(int x, int y, int destX, int destY, int dir, float speed)
{
    if (mDead) return;

    // 출발 칸에서 다시 시작
    mPx = (float)x;
    mPy = (float)y;
    mNetDestX = (float)destX;
    mNetDestY = (float)destY;
    mSpeed = speed;

    if (dir == IN_UP) mFacing = Dir::Up;
    else if (dir == IN_DOWN) mFacing = Dir::Down;
    else if (dir == IN_LEFT) mFacing = Dir::Left;
    else if (dir == IN_RIGHT) mFacing = Dir::Right;
}

// 서버와 같은 빠르기로 목적 칸까지 간다
void Player::UpdateNetMove(float deltaTime)
{
    if (mDead) {
        mDieMs += deltaTime * 1000;
        return;
    }
    if (mHitTimerMs > 0) mHitTimerMs -= (int)(deltaTime * 1000);

    if (mPx == mNetDestX && mPy == mNetDestY) return;   // 도착해서 멈춤

    float step = mSpeed * (1000.0f / 96.0f) * deltaTime;
    mPx = MoveToward(mPx, mNetDestX, step);
    mPy = MoveToward(mPy, mNetDestY, step);

    // 걷기 그림 넘기기
    mNetAnimMs += deltaTime * 1000;
    while (mNetAnimMs >= Config::FixedStepMs) {
        mNetAnimMs -= Config::FixedStepMs;
        mAnimFrame = (mAnimFrame + 1) % 16;
    }
}

void Player::Render(HDC hdc) const
{
    if (mDead) {
        RenderDie(hdc);
        return;
    }
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

void Player::RenderDie(HDC hdc) const
{
    int index = kDieFrameCount - 1;
    float elapsed = 0;
    for (int i = 0; i < kDieFrameCount - 1; ++i) {
        elapsed += static_cast<float>(kDieFrames[i].ms);
        if (mDieMs < elapsed) {
            index = i;
            break;
        }
    }
    const DieFrame& f = kDieFrames[index];

    // 걷는 그림과 발 위치를 맞춘다
    const int centerX = Config::MapOriginX + static_cast<int>(mPx * Config::TileSize) + Config::TileHalf;
    const int feetY = Config::MapOriginY + static_cast<int>(mPy * Config::TileSize) + 30;

    int w = kDieDrawW;
    int h = kDieDrawH;
    int feet = kDieFeetY;
    if (f.row == 0 || f.col == 0) {   // 물방울에 갇히고 터지는 장면
        w = kBubbleSize;
        h = kBubbleSize;
        feet = kBubbleFeetY;
    }
    const int x = centerX - w / 2;
    const int y = feetY - feet;

    if (index >= kDieFrameCount - 2) {
        Gfx::Draw(hdc, mDieSprite, x, y, w, h,
            kShadowCol * kDieCell, kShadowRow * kDieCell, kDieCell, kDieCell);
    }
    Gfx::Draw(hdc, mDieSprite, x, y, w, h,
        f.col * kDieCell, f.row * kDieCell, kDieCell, kDieCell);
    if (f.overlayCol >= 0) {
        Gfx::Draw(hdc, mDieSprite, x, y, w, h,
            f.overlayCol * kDieCell, f.overlayRow * kDieCell, kDieCell, kDieCell);
    }
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
