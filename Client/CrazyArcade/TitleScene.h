#pragma once
#include "Scene.h"

// 타이틀 화면
class TitleScene : public Scene {
public:
    void OnEnter() override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
};
