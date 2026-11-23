#pragma once
#include "Scene.h"
#include "UiParts.h"

// 로비 화면 (방 목록)
class LobbyScene : public Scene {
public:
    void OnEnter() override;
    void OnExit() override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;

private:
    void DrawRooms(HDC c) const;
    void DrawHeader(HDC c) const;
    void DrawProfile(HDC c) const;
    void DrawSelectedRoom(HDC c) const;
    void DrawButtons(HDC c) const;

    // 방 칸 클릭 영역
    static RECT RoomRect(int index);
    void SelectRoom(int index);
    void Refresh();       // 새로고침 버튼
    void QuickEnter();    // 빠른 입장
    void TryEnter();      // 입장
    void UpdateJoin();    // 입장 결과 처리

    Ui::Canvas mCanvas;
    Ui::ChatView mChat;
    Ui::ChatInput mInput;
    int mSelected = -1;   // 고른 방 index (0~7), -1 = 없음
    int mHover = -1;      // 마우스가 올라간 방 index, -1 = 없음
};
