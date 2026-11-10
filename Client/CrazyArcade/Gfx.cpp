#include "Gfx.h"
#pragma comment(lib, "msimg32.lib")

namespace {
    HDC sMemDC = nullptr;
    HFONT sTextFont = nullptr;
    HFONT sSmallFont = nullptr;
    HPEN sTextPen = nullptr;
    HBRUSH sTextBrush = nullptr;
    HBITMAP sBlackPixel = nullptr;

    HDC MemDC()
    {
        if (!sMemDC) sMemDC = CreateCompatibleDC(nullptr);
        return sMemDC;
    }

    void RGBtoHSV(BYTE r, BYTE g, BYTE b, double& h, double& s, double& v)
    {
        double R = r / 255.0, G = g / 255.0, B = b / 255.0;
        double maxc = std::max(R, std::max(G, B));
        double minc = std::min(R, std::min(G, B));
        double delta = maxc - minc;
        v = maxc;
        if (delta == 0) { h = 0; s = 0; return; }
        s = delta / maxc;
        if (maxc == R)      h = fmod((G - B) / delta, 6.0);
        else if (maxc == G) h = (B - R) / delta + 2;
        else                h = (R - G) / delta + 4;
        h *= 60;
        if (h < 0) h += 360;
    }

    void HSVtoRGB(double h, double s, double v, BYTE& r, BYTE& g, BYTE& b)
    {
        double C = v * s;
        double X = C * (1 - fabs(fmod(h / 60.0, 2) - 1));
        double m = v - C;
        double r1, g1, b1;
        if (h < 60)       { r1 = C; g1 = X; b1 = 0; }
        else if (h < 120) { r1 = X; g1 = C; b1 = 0; }
        else if (h < 180) { r1 = 0; g1 = C; b1 = X; }
        else if (h < 240) { r1 = 0; g1 = X; b1 = C; }
        else if (h < 300) { r1 = X; g1 = 0; b1 = C; }
        else              { r1 = C; g1 = 0; b1 = X; }
        r = BYTE((r1 + m) * 255);
        g = BYTE((g1 + m) * 255);
        b = BYTE((b1 + m) * 255);
    }
}

void Gfx::Draw(HDC dst, HBITMAP bitmap, int x, int y, int w, int h, int srcX, int srcY, int srcW, int srcH)
{
    if (!bitmap) return;
    HDC mem = MemDC();
    HGDIOBJ old = SelectObject(mem, bitmap);
    TransparentBlt(dst, x, y, w, h, mem, srcX, srcY, srcW, srcH, Config::ColorKey);
    SelectObject(mem, old);
}

void Gfx::Draw(HDC dst, BitmapID id, int x, int y, int w, int h, int srcX, int srcY, int srcW, int srcH)
{
    Draw(dst, ResourceManager::Get(id), x, y, w, h, srcX, srcY, srcW, srcH);
}

void Gfx::DrawOpaque(HDC dst, BitmapID id, int x, int y, int w, int h, int srcX, int srcY, int srcW, int srcH)
{
    HBITMAP bitmap = ResourceManager::Get(id);
    if (!bitmap) return;
    HDC mem = MemDC();
    HGDIOBJ old = SelectObject(mem, bitmap);
    StretchBlt(dst, x, y, w, h, mem, srcX, srcY, srcW, srcH, SRCCOPY);
    SelectObject(mem, old);
}

void Gfx::DrawOutlinedText(HDC hdc, int x, int y, const std::wstring& text)
{
    if (!sTextFont) sTextFont = CreateFontW(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"맑은 고딕");
    if (!sTextPen) sTextPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
    if (!sTextBrush) sTextBrush = CreateSolidBrush(RGB(255, 165, 0));

    HGDIOBJ oldFont = SelectObject(hdc, sTextFont);
    HGDIOBJ oldPen = SelectObject(hdc, sTextPen);
    HGDIOBJ oldBrush = SelectObject(hdc, sTextBrush);
    int oldBkMode = SetBkMode(hdc, TRANSPARENT);

    // 글자 외곽선을 경로로 만들어 테두리(검정) + 채우기(주황)
    BeginPath(hdc);
    TextOutW(hdc, x, y, text.c_str(), static_cast<int>(text.size()));
    EndPath(hdc);
    StrokeAndFillPath(hdc);

    SetBkMode(hdc, oldBkMode);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldFont);
}

void Gfx::DrawSmallText(HDC hdc, int x, int y, const std::wstring& text, COLORREF color)
{
    if (!sSmallFont) sSmallFont = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"맑은 고딕");

    HGDIOBJ oldFont = SelectObject(hdc, sSmallFont);
    int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    COLORREF oldColor = SetTextColor(hdc, RGB(0, 0, 0));
    // 어떤 배경 위에서도 읽히도록 검은 그림자 한 번 + 본문
    TextOutW(hdc, x + 1, y + 1, text.c_str(), static_cast<int>(text.size()));
    SetTextColor(hdc, color);
    TextOutW(hdc, x, y, text.c_str(), static_cast<int>(text.size()));

    SetTextColor(hdc, oldColor);
    SetBkMode(hdc, oldBkMode);
    SelectObject(hdc, oldFont);
}

void Gfx::DrawDarkOverlay(HDC hdc, int width, int height)
{
    // 1x1 검정 비트맵을 화면 크기로 늘려 반투명 합성 (매 프레임 큰 비트맵 생성 방지)
    if (!sBlackPixel) {
        HDC screen = GetDC(nullptr);
        sBlackPixel = CreateCompatibleBitmap(screen, 1, 1);
        ReleaseDC(nullptr, screen);
        HDC mem = MemDC();
        HGDIOBJ old = SelectObject(mem, sBlackPixel);
        SetPixel(mem, 0, 0, RGB(0, 0, 0));
        SelectObject(mem, old);
    }
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 128, 0 };
    HDC mem = MemDC();
    HGDIOBJ old = SelectObject(mem, sBlackPixel);
    AlphaBlend(hdc, 0, 0, width, height, mem, 0, 0, 1, 1, blend);
    SelectObject(mem, old);
}

HBITMAP Gfx::CreateHueShiftedCopy(HBITMAP source, double hueShift, double hueMin, double hueMax)
{
    BITMAP bm;
    if (!source || !GetObject(source, sizeof(bm), &bm)) return nullptr;

    // 32비트 DIB라 행 패딩 없이 픽셀 = DWORD 하나
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bm.bmWidth;
    bmi.bmiHeader.biHeight = -bm.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP copy = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!copy || !bits) return nullptr;

    HDC srcDC = CreateCompatibleDC(nullptr);
    HDC dstDC = CreateCompatibleDC(nullptr);
    HGDIOBJ oldSrc = SelectObject(srcDC, source);
    HGDIOBJ oldDst = SelectObject(dstDC, copy);
    BitBlt(dstDC, 0, 0, bm.bmWidth, bm.bmHeight, srcDC, 0, 0, SRCCOPY);
    SelectObject(srcDC, oldSrc);
    SelectObject(dstDC, oldDst);
    DeleteDC(srcDC);
    DeleteDC(dstDC);
    GdiFlush();

    if (hueShift == 0) return copy;

    BYTE* p = static_cast<BYTE*>(bits);
    const int pixelCount = bm.bmWidth * bm.bmHeight;
    for (int i = 0; i < pixelCount; ++i, p += 4) {
        BYTE& B = p[0];
        BYTE& G = p[1];
        BYTE& R = p[2];
        if (R == 255 && G == 0 && B == 255) continue; // 투명색 유지

        double h, s, v;
        RGBtoHSV(R, G, B, h, s, v);
        if (h < hueMin || h > hueMax) continue;
        h = fmod(h + hueShift, 360.0);
        HSVtoRGB(h, s, v, R, G, B);
    }
    return copy;
}

void Gfx::Release()
{
    if (sMemDC) DeleteDC(sMemDC);
    if (sTextFont) DeleteObject(sTextFont);
    if (sSmallFont) DeleteObject(sSmallFont);
    if (sTextPen) DeleteObject(sTextPen);
    if (sTextBrush) DeleteObject(sTextBrush);
    if (sBlackPixel) DeleteObject(sBlackPixel);
    sMemDC = nullptr;
    sTextFont = nullptr;
    sSmallFont = nullptr;
    sTextPen = nullptr;
    sTextBrush = nullptr;
    sBlackPixel = nullptr;
}
