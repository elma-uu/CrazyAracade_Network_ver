#pragma once
#include "Scene.h"

// 색상 선택(P1: A/D, P2: ←/→) + 스테이지 선택(1/2/3)
class SelectScene : public Scene {
public:
    void OnEnter() override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;

private:
    int mColor[2] = { 0, 0 };
};
