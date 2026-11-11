#pragma once
#include "Common.h"
#include "ResourceManager.h"

// CreateCompatibleDC / SelectObject / TransparentBlt / 복구 / DeleteDC 반복을 한 곳으로 모은 GDI 헬퍼
namespace Gfx {
    // 마젠타(Config::ColorKey)를 투명색으로 그린다
    void Draw(HDC dst, HBITMAP bitmap, int x, int y, int w, int h, int srcX, int srcY, int srcW, int srcH);
    void Draw(HDC dst, BitmapID id, int x, int y, int w, int h, int srcX, int srcY, int srcW, int srcH);
    // 투명색 없이 늘려 그린다 (배경용)
    void DrawOpaque(HDC dst, BitmapID id, int x, int y, int w, int h, int srcX, int srcY, int srcW, int srcH);

    void DrawOutlinedText(HDC hdc, int x, int y, const std::wstring& text);
    // 안내 문구·입력칸용 작은 글씨 (그림자 포함)
    void DrawSmallText(HDC hdc, int x, int y, const std::wstring& text, COLORREF color = RGB(255, 255, 255));
    // 화면 가운데 띠 모양 안내 (접속 실패·끊김 등)
    void DrawNoticeBar(HDC hdc, const std::wstring& text);
    void DrawDarkOverlay(HDC hdc, int width, int height);

    // 32비트 미리 곱한 알파 비트맵(ResourceManager에서 dib로 읽은 것)을 반투명 경계 그대로 그린다 (1:1)
    void DrawAlpha(HDC dst, BitmapID id, int x, int y, int srcX, int srcY, int w, int h);
    // 사각형 영역을 color로 alpha(0~255)만큼 덮는다 (들어갈 수 없는 칸 어둡게 등)
    void FillAlpha(HDC hdc, const RECT& rc, COLORREF color, BYTE alpha);
    // 둥근 테두리만 그린다 (선택 표시)
    void DrawRoundFrame(HDC hdc, const RECT& rc, int radius, COLORREF color, int width);

    // ---- 픽셀 글씨 (돋움 Bold, 안티앨리어싱 없음, 테두리) ----
    // 원작 UI 글씨에 맞춘 글씨. 실제로 찍히는 픽셀 범위를 재서 box 안에 정확히 놓는다.
    // 세로는 기준 글자 '한'의 높이로 맞춰서 숫자만 있는 줄도 한글 줄과 같은 높이에 온다.
    enum class Align { Left, Center, Right };
    void DrawPixelText(HDC hdc, const RECT& box, const std::wstring& text, COLORREF color,
        Align align = Align::Center, int size = 12, COLORREF outline = RGB(8, 36, 90), int outlineWidth = 1);

    // 지정한 색상 범위(hue)의 픽셀만 hueShift만큼 돌린 새 비트맵을 만든다. 호출자가 DeleteObject 해야 함
    HBITMAP CreateHueShiftedCopy(HBITMAP source, double hueShift, double hueMin, double hueMax);

    void Release();
}
