#include "Player.h"
#include "TileMap.h"
#include "Input.h"
#include "Gfx.h"
#include "GameData.h"

namespace {
    // 색 선택 칸마다 돌릴 색상(hue) 각도: 빨강, 주황, 노랑, 초록, 민트, 파랑, 보라
    const double kColorHueTable[GameData::ColorCount] = { 0, 30, 60, 120, 150, 210, 270 };
    constexpr int kHitBlinkMs = 500;

    // 원본과 같은 빠르기: 12ms마다 speed/8칸 = speed 1.0일 때 1초에 약 10.4칸
    constexpr float kTilesPerSecond = 1000.0f / (8.0f * Config::FixedStepMs);
    // 이보다 덜 걸쳐 있으면 모서리 보정을 하지 않는다 (딱 중앙일 때 좌우로 떨리지 않게)
    constexpr float kSlideEpsilon = 0.01f;
}

bool Player::sShowFootDebug = false;   // [디버그] F3으로 켬

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

    Dir dir = GetHeldDirection();
    if (dir == Dir::None) return;
    mFacing = dir;

    // 고정 스텝 한 번 = 0.012초. 서버도 같은 값으로 계산하면 결과가 똑같다
    const float dt = Config::FixedStepMs / 1000.0f;
    if (Move(dir, mSpeed * kTilesPerSecond * dt))
        mAnimFrame = (mAnimFrame + 1) % 16;   // 실제로 움직일 때만 걷는 모습
}

bool Player::IsOpen(bool horizontal, int mainTile, int sideTile) const
{
    // 맵 밖·블록·물풍선은 막힘 (TileMap::IsWalkable은 (행, 열) 순서)
    if (horizontal) return mMap.IsWalkable(sideTile, mainTile);
    return mMap.IsWalkable(mainTile, sideTile);
}

bool Player::Move(Dir dir, float distance)
{
    // main = 움직이는 축, side = 그 옆 축 (걸치기가 일어나는 축)
    bool horizontal = false;
    int sign = 1;   // +1 = 오른쪽/아래, -1 = 왼쪽/위
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

    const int footMain = FootTile(*main);
    const int footSide = FootTile(*side);

    if (IsOpen(horizontal, footMain + sign, footSide)) {
        // 앞 칸이 비었으면 그대로 간다 (옆으로 걸쳐 있으면 걸친 채로)
        *main += sign * distance;
    } else {
        // 앞이 막혔으면 지금 칸 중앙까지만 간다.
        // 이미 중앙을 지났으면 뒤로 당기지 않고 그 자리에 멈춘다.
        // 서 있는 칸은 검사하지 않으므로 막 설치한 물풍선에서는 빠져나갈 수 있다
        const float center = static_cast<float>(footMain);
        if (sign > 0) {
            if (*main < center) *main = std::min(*main + distance, center);
        } else {
            if (*main > center) *main = std::max(*main - distance, center);
        }

        // 모서리 보정: 옆 줄로 걸쳐 있고, 그 옆 줄의 앞이 비어 있으면 그쪽으로 미끄러져 들어간다
        const float offset = *side - footSide;   // -0.5 ~ +0.5
        if (std::fabs(offset) > kSlideEpsilon) {
            int sideDir = 1;
            if (offset < 0) sideDir = -1;
            if (IsOpen(horizontal, footMain, footSide + sideDir) &&
                IsOpen(horizontal, footMain + sign, footSide + sideDir)) {
                *side += sideDir * distance;
            }
        }
    }
    return *main != mainBefore || *side != sideBefore;
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

    if (sShowFootDebug) RenderFootDebug(hdc);
}

void Player::RenderFootDebug(HDC hdc) const
{
    // 지금 판정에 쓰는 칸 (GetRow/GetCol) = 하늘색 테두리
    RECT tile = { Config::MapOriginX + GetCol() * Config::TileSize,
                  Config::MapOriginY + GetRow() * Config::TileSize, 0, 0 };
    tile.right = tile.left + Config::TileSize;
    tile.bottom = tile.top + Config::TileSize;
    HBRUSH cyan = CreateSolidBrush(RGB(0, 255, 255));
    FrameRect(hdc, &tile, cyan);
    RECT inner = { tile.left + 1, tile.top + 1, tile.right - 1, tile.bottom - 1 };
    FrameRect(hdc, &inner, cyan);
    DeleteObject(cyan);

    // 발밑 중심점 = 실수 좌표(mPx, mPy) + 반 칸 = 칸 단위 (mPx + 0.5, mPy + 0.5)
    const int fx = Config::MapOriginX + static_cast<int>((mPx + 0.5f) * Config::TileSize);
    const int fy = Config::MapOriginY + static_cast<int>((mPy + 0.5f) * Config::TileSize);
    constexpr int r = 4;
    HBRUSH neon = CreateSolidBrush(RGB(57, 255, 20));
    HPEN outline = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));   // 어떤 바닥 위에서도 보이게 검은 테두리
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
