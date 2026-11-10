#pragma once
#include "Scene.h"

// 로그인: 닉네임을 입력하고 서버에 접속한다.
// ESC = 타이틀로, F9 = 기존 한 컴퓨터 2인 모드(스테이지 선택)로
class LoginScene : public Scene {
public:
    void OnEnter() override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
};
