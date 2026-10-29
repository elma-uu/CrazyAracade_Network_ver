#pragma once
#include "Common.h"

// 창 생성, 메시지 루프, 백버퍼, 프레임 진행(Input → Time → Scene Update → FixedUpdate → Render)
class Game {
public:
    static bool Initialize(HINSTANCE hInstance, int nCmdShow);
    static int Run();
    static void Quit();

    static HWND GetHwnd() { return sHwnd; }

private:
    static void Frame();
    static void Present(HDC hdc);
    static void Shutdown();
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    static HWND sHwnd;
    static HDC sBackDC;
    static HBITMAP sBackBitmap;
    static HGDIOBJ sOldBackBitmap;
    static float sFixedAccumulatorMs;
};
