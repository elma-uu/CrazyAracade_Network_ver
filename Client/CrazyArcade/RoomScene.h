#pragma once
#include "Scene.h"
#include "UiParts.h"
#include "GameClient.h"

// 대기방 화면
class RoomScene : public Scene {
public:
    void OnEnter() override;
    void OnExit() override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;

private:
    void DrawSlots(HDC c) const;
    void DrawHeader(HDC c) const;
    void DrawMapSelect(HDC c) const;
    void DrawColors(HDC c) const;
    void DrawRoomInfo(HDC c) const;
    void DrawButtons(HDC c) const;

    void Leave();
    void PressStart();        // 방장은 시작, 나머지는 준비
    void LogRoomChanges();    // 방 변화 알림

    Ui::Canvas mCanvas;
    GameClient::RoomState mPrev;   // 지난 프레임 방 상태
    bool mHasPrev = false;
    Ui::ChatView mChat;
    Ui::ChatInput mInput;
    int mHoverMap = -1;      // 0~2 = 숲·도시·얼음
    int mHoverColor = -1;    // 0~6
    int mHoverButton = -1;   // 0 = 방 나가기, 1 = 시작/준비
};
