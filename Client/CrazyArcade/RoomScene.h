#pragma once
#include "Scene.h"
#include "UiParts.h"

// 대기방: 로비에서 방에 들어오면 온다. 방 상태(S2C_RoomInfo)를 GameClient에서 읽어 그린다.
// 위 줄 4칸 = 자리 1~4P, 오른쪽 위 = 맵 선택(방장), 가운데 = 색 선택, 아래 = 시작(방장) / 준비
// ESC 또는 '방 나가기' = 로비로
class RoomScene : public Scene {
public:
    void OnEnter() override;
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

    Ui::Canvas mCanvas;
    Ui::ChatLog mLog;
    int mHoverMap = -1;      // 0~2 = 숲·도시·얼음
    int mHoverColor = -1;    // 0~6
    int mHoverButton = -1;   // 0 = 방 나가기, 1 = 시작/준비
};
