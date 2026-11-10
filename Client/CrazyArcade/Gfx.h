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
    void DrawDarkOverlay(HDC hdc, int width, int height);

    // 지정한 색상 범위(hue)의 픽셀만 hueShift만큼 돌린 새 비트맵을 만든다. 호출자가 DeleteObject 해야 함
    HBITMAP CreateHueShiftedCopy(HBITMAP source, double hueShift, double hueMin, double hueMax);

    void Release();
}
