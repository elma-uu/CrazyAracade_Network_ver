#pragma once
#include "GameObject.h"

class Player;
class TileMap;
struct Tile;

class Bomb : public GameObject {
public:
    enum class State { Fuse, Exploding };

    // 놓는 순간 타일을 Obstacle::Bomb으로 막는다
    Bomb(Player& owner, TileMap& map);

    void FixedUpdate() override;
    void Render(HDC hdc) const override;

    // 이번 스텝에 터졌는지 (FixedUpdate 직후 한 번만 true)
    bool JustExploded() const { return mJustExploded; }
    bool IsExploding() const { return mState == State::Exploding; }

    // 물줄기를 사방으로 뻗는다. 블록은 부수고, 다른 장애물에 막히면 onBlocked 호출
    void Explode(const std::function<void(Tile&)>& onBlocked);

    bool Covers(int row, int col) const;
    // 한 폭탄은 같은 플레이어를 한 번만 맞힌다
    bool TryHit(const Player& victim);

    Player& GetOwner() const { return mOwner; }

private:
    Player& mOwner;
    TileMap& mMap;
    const int mRow, mCol;
    const int mRange;
    State mState = State::Fuse;
    int mTimerMs;
    bool mJustExploded = false;
    unsigned mHitMask = 0;
    int mReach[4] = {};  // Dir::Up/Down/Left/Right 방향 물줄기 길이
};
