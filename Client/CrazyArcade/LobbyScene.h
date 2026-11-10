#pragma once
#include "Scene.h"

// 로비: 로그인 성공 후 들어온다. 방 목록·방 선택은 11/11~11/12 작업
// ESC = 로그아웃(접속 끊고 타이틀로)
class LobbyScene : public Scene {
public:
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
};
