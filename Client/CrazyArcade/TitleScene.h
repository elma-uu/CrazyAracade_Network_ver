#pragma once
#include "Scene.h"

// 타이틀: 왼쪽 클릭 = 시작(로그인 화면으로), 오른쪽 클릭 = 종료
class TitleScene : public Scene {
public:
    void OnEnter() override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
};
