#pragma once
#include "Common.h"

class Scene {
public:
    virtual ~Scene() = default;

    virtual void OnEnter() {}
    virtual void OnExit() {}

    // 매 프레임 1회: 입력의 Down/Up 같은 순간 이벤트 처리
    virtual void Update(float deltaTime) {}
    // 고정 스텝(Config::FixedStepMs)마다: 이동, 폭탄, 충돌 같은 게임 로직
    virtual void FixedUpdate() {}
    virtual void Render(HDC hdc) = 0;
};
