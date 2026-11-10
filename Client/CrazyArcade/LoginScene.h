#pragma once
#include "Scene.h"

// 타이틀: 왼쪽 클릭 = 시작, 오른쪽 클릭 = 종료
class LoginScene : public Scene {
public:
    void OnEnter() override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
};
