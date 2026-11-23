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
    // 작은 글씨
    void DrawSmallText(HDC hdc, int x, int y, const std::wstring& text, COLORREF color = RGB(255, 255, 255));
    // 가운데 안내 띠
    void DrawNoticeBar(HDC hdc, const std::wstring& text);
    void DrawDarkOverlay(HDC hdc, int width, int height);

    // 알파 비트맵 그리기
    void DrawAlpha(HDC dst, BitmapID id, int x, int y, int srcX, int srcY, int w, int h);
    // 반투명으로 덮기
    void FillAlpha(HDC hdc, const RECT& rc, COLORREF color, BYTE alpha);
    // 둥근 테두리
    void DrawRoundFrame(HDC hdc, const RECT& rc, int radius, COLORREF color, int width);

    // 원작 UI 느낌의 픽셀 글씨
    enum class Align { Left, Center, Right };
    // 글씨 폭과 줄바꿈용 글자 수
    int PixelTextWidth(const std::wstring& text, int size = 12);
    int PixelTextFit(const std::wstring& text, int maxWidth, int size = 12);
    void DrawPixelText(HDC hdc, const RECT& box, const std::wstring& text, COLORREF color,
        Align align = Align::Center, int size = 12, COLORREF outline = RGB(8, 36, 90), int outlineWidth = 1);

    // 지정한 색상 범위(hue)의 픽셀만 hueShift만큼 돌린 새 비트맵을 만든다. 호출자가 DeleteObject 해야 함
    HBITMAP CreateHueShiftedCopy(HBITMAP source, double hueShift, double hueMin, double hueMax);

    void Release();
}
