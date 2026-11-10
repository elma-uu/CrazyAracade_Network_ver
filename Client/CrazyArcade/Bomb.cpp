#include "Bomb.h"
#include "Player.h"
#include "TileMap.h"
#include "Gfx.h"

namespace {
    constexpr int kFuseMs = 1000;
    constexpr int kFlameMs = 500;
    constexpr int kAnimFrameMs = 50;

    struct Offset { int dRow, dCol; };
    const Offset kOffsets[4] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } }; // Up, Down, Left, Right
}

Bomb::Bomb(Player& owner, TileMap& map)
    : mOwner(owner), mMap(map), mRow(owner.GetRow()), mCol(owner.GetCol()),
      mRange(owner.GetBombRange()), mTimerMs(kFuseMs)
{
    mMap.At(mRow, mCol).obstacle = Obstacle::Bomb;
}

void Bomb::FixedUpdate()
{
    mJustExploded = false;
    mTimerMs -= Config::FixedStepMs;
    if (mTimerMs > 0) return;

    if (mState == State::Fuse) {
        mState = State::Exploding;
        mTimerMs = kFlameMs;
        mJustExploded = true;
    } else {
        Destroy();
    }
}

void Bomb::Explode(const std::function<void(Tile&)>& onBlocked)
{
    mMap.At(mRow, mCol).obstacle = Obstacle::Empty;

    for (int d = 0; d < 4; ++d) {
        int reach = 0;
        for (int step = 1; step <= mRange; ++step) {
            int row = mRow + kOffsets[d].dRow * step;
            int col = mCol + kOffsets[d].dCol * step;
            if (!TileMap::InBounds(row, col)) break;

            Tile& tile = mMap.At(row, col);
            if (tile.obstacle == Obstacle::Box) {
                tile.obstacle = Obstacle::Empty;  // 안에 있던 아이템이 드러난다
                reach = step;
                break;
            }
            if (tile.obstacle != Obstacle::Empty && tile.obstacle != Obstacle::Bomb) {
                if (onBlocked) onBlocked(tile);
                break;
            }
            reach = step;
        }
        mReach[d] = reach;
    }
}

bool Bomb::Covers(int row, int col) const
{
    if (mState != State::Exploding) return false;
    if (row == mRow) return col >= mCol - mReach[2] && col <= mCol + mReach[3];
    if (col == mCol) return row >= mRow - mReach[0] && row <= mRow + mReach[1];
    return false;
}

bool Bomb::TryHit(const Player& victim)
{
    unsigned bit = 1u << victim.GetIndex();
    if ((mHitMask & bit) || !Covers(victim.GetRow(), victim.GetCol())) return false;
    mHitMask |= bit;
    return true;
}

void Bomb::Render(HDC hdc) const
{
    POINT c = TileMap::TileCenter(mRow, mCol);
    int tick = mTimerMs / kAnimFrameMs;

    auto draw = [&](BitmapID id, int dRow, int dCol, int frame, int stride, int srcSize) {
        Gfx::Draw(hdc, id, c.x + dCol * Config::TileSize - Config::TileHalf, c.y + dRow * Config::TileSize - Config::TileHalf,
            Config::TileSize, Config::TileSize, frame * stride, 0, srcSize, srcSize);
    };

    if (mState == State::Fuse) {
        draw(BitmapID::BombSet, 0, 0, tick % 4, 56, 54);
        return;
    }

    static const BitmapID kFlames[4] = { BitmapID::BombUp, BitmapID::BombDown, BitmapID::BombLeft, BitmapID::BombRight };
    int frame = tick % 3;
    draw(BitmapID::BombCenter, 0, 0, frame, 52, 52);
    for (int d = 0; d < 4; ++d)
        for (int step = 1; step <= mReach[d]; ++step)
            draw(kFlames[d], kOffsets[d].dRow * step, kOffsets[d].dCol * step, frame, 52, 52);
}
