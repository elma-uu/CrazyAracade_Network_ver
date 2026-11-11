#pragma once
#include "Scene.h"
#include "UiParts.h"

// 로비: 로그인 성공 후 들어온다. 방 1~8번 목록(S2C_RoomList)을 보여 준다.
// 화면은 원작 UI 크기(800x600) 캔버스에 그린 뒤 전체 화면으로 늘린다 (타이틀·로그인과 같은 방식).
// ESC = 로그아웃(접속 끊고 타이틀로)
class LobbyScene : public Scene {
public:
    void OnEnter() override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;

private:
    void DrawRooms(HDC c) const;
    void DrawHeader(HDC c) const;
    void DrawProfile(HDC c) const;
    void DrawSelectedRoom(HDC c) const;
    void DrawButtons(HDC c) const;

    // 방 칸(그림+이름+상태) 클릭 영역, 캔버스 좌표
    static RECT RoomRect(int index);
    void SelectRoom(int index);
    void Refresh();       // 새로고침 버튼
    void QuickEnter();    // 빠른 입장: 사람 있는 대기 방 → 빈 방 순서로 골라 입장
    void TryEnter();      // 입장 버튼 / Enter

    Ui::Canvas mCanvas;
    Ui::ChatLog mLog;
    int mSelected = -1;   // 고른 방 index (0~7), -1 = 없음
    int mHover = -1;      // 마우스가 올라간 방 index, -1 = 없음
};
