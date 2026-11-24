#include "Gfx.h"
#include <map>
#pragma comment(lib, "msimg32.lib")

namespace {
    HDC sMemDC = nullptr;
    HBITMAP sTintPixel = nullptr;        // FillAlpha용 1x1 32비트
    DWORD* sTintBits = nullptr;
    std::map<int, HFONT> sPixelFonts;    // 크기별 픽셀 폰트
    std::map<std::pair<int, std::wstring>, RECT> sInkCache;   // 글씨별 실제 픽셀 범위
    HFONT sTextFont = nullptr;
    HFONT sSmallFont = nullptr;
    HPEN sTextPen = nullptr;
    HBRUSH sTextBrush = nullptr;
    HBITMAP sBlackPixel = nullptr;

    // 작은 캔버스에 그린 글씨는 늘리면 뭉개져서, 모아 뒀다가 늘린 뒤 화면에 그린다
    struct PendingText {
        RECT box;
        std::wstring text;
        COLORREF color, outline;
        Gfx::Align align;
        int size, outlineWidth;
    };
    HDC sDeferDC = nullptr;
    double sScaleX = 1.0, sScaleY = 1.0;
    std::vector<PendingText> sPending;

    HDC MemDC()
    {
        if (!sMemDC) sMemDC = CreateCompatibleDC(nullptr);
        return sMemDC;
    }

    HFONT SmallFont()
    {
        if (!sSmallFont) sSmallFont = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"맑은 고딕");
        return sSmallFont;
    }

    // Neo둥근모 Pro: exe 옆 Resource\Font 에서 이 프로그램 안에서만 쓰게 등록
    std::wstring sFontPath;
    bool sFontAdded = false;

    void AddPixelFontFile()
    {
        if (!sFontPath.empty()) return;
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        sFontPath = path;
        sFontPath = sFontPath.substr(0, sFontPath.find_last_of(L"\\/") + 1) + L"Resource\\Font\\NeoDunggeunmoPro-Regular.ttf";
        sFontAdded = AddFontResourceExW(sFontPath.c_str(), FR_PRIVATE, nullptr) > 0;
    }

    HFONT PixelFont(int size)
    {
        AddPixelFontFile();
        HFONT& font = sPixelFonts[size];
        // 폰트 파일이 없으면 돋움으로
        if (!font) font = CreateFontW(-size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, sFontAdded ? L"NeoDunggeunmo Pro" : L"돋움");
        return font;
    }

    // 실제로 칠해지는 픽셀 범위
    RECT InkBox(int size, const std::wstring& text)
    {
        auto key = std::make_pair(size, text);
        auto it = sInkCache.find(key);
        if (it != sInkCache.end()) return it->second;

        HDC dc = CreateCompatibleDC(nullptr);
        HGDIOBJ oldFont = SelectObject(dc, PixelFont(size));
        SIZE ext = {};
        GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &ext);
        const int pad = size;
        const int w = ext.cx + pad * 2, h = ext.cy + pad * 2;

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        HGDIOBJ oldBmp = SelectObject(dc, dib);
        RECT all = { 0, 0, w, h };
        FillRect(dc, &all, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        TextOutW(dc, pad, pad, text.c_str(), static_cast<int>(text.size()));
        GdiFlush();

        RECT ink = { w, h, -1, -1 };
        const DWORD* px = static_cast<const DWORD*>(bits);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                if (px[y * w + x] & 0x00FFFFFF) {
                    ink.left = std::min<LONG>(ink.left, x);  ink.right = std::max<LONG>(ink.right, x + 1);
                    ink.top = std::min<LONG>(ink.top, y);    ink.bottom = std::max<LONG>(ink.bottom, y + 1);
                }
        if (ink.right < 0) ink = { pad, pad, pad, pad };   // 공백뿐인 글씨
        OffsetRect(&ink, -pad, -pad);

        SelectObject(dc, oldBmp);
        SelectObject(dc, oldFont);
        DeleteObject(dib);
        DeleteDC(dc);
        sInkCache[key] = ink;
        return ink;
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

void Gfx::DrawAlpha(HDC dst, BitmapID id, int x, int y, int srcX, int srcY, int w, int h)
{
    HBITMAP bitmap = ResourceManager::Get(id);
    if (!bitmap) return;
    HDC mem = MemDC();
    HGDIOBJ old = SelectObject(mem, bitmap);
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(dst, x, y, w, h, mem, srcX, srcY, w, h, blend);
    SelectObject(mem, old);
}

void Gfx::FillAlpha(HDC hdc, const RECT& rc, COLORREF color, BYTE alpha)
{
    if (!sTintPixel) {
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = 1;
        bmi.bmiHeader.biHeight = 1;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        sTintPixel = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        sTintBits = static_cast<DWORD*>(bits);
    }
    if (!sTintPixel) return;
    GdiFlush();
    *sTintBits = (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color);
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, alpha, 0 };
    HDC mem = MemDC();
    HGDIOBJ old = SelectObject(mem, sTintPixel);
    AlphaBlend(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, mem, 0, 0, 1, 1, blend);
    SelectObject(mem, old);
}

void Gfx::DrawRoundFrame(HDC hdc, const RECT& rc, int radius, COLORREF color, int width)
{
    HPEN pen = CreatePen(PS_INSIDEFRAME, width, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

int Gfx::PixelTextWidth(const std::wstring& text, int size)
{
    if (text.empty()) return 0;
    HDC dc = MemDC();
    HGDIOBJ old = SelectObject(dc, PixelFont(size));
    SIZE ext = {};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &ext);
    SelectObject(dc, old);
    return ext.cx;
}

int Gfx::PixelTextFit(const std::wstring& text, int maxWidth, int size)
{
    if (text.empty()) return 0;
    HDC dc = MemDC();
    HGDIOBJ old = SelectObject(dc, PixelFont(size));
    int fit = 0;
    SIZE ext = {};
    GetTextExtentExPointW(dc, text.c_str(), static_cast<int>(text.size()), maxWidth, &fit, nullptr, &ext);
    SelectObject(dc, old);
    return fit;
}

void Gfx::DrawPixelText(HDC hdc, const RECT& box, const std::wstring& text, COLORREF color,
    Align align, int size, COLORREF outline, int outlineWidth)
{
    if (text.empty()) return;
    if (sDeferDC && hdc == sDeferDC) {
        sPending.push_back({ box, text, color, outline, align, size, outlineWidth });
        return;
    }
    const int ow = outlineWidth;
    const RECT ink = InkBox(size, text);
    const RECT ref = InkBox(size, L"한");
    const int inkW = (ink.right - ink.left) + ow * 2;
    const int refH = (ref.bottom - ref.top) + ow * 2;
    const int boxW = box.right - box.left, boxH = box.bottom - box.top;

    int left;   // 테두리까지 포함한 글씨 왼쪽 끝
    switch (align) {
    case Align::Left:  left = box.left + 1; break;
    case Align::Right: left = box.right - 1 - inkW; break;
    default:           left = box.left + (boxW - inkW) / 2; break;
    }
    const int top = box.top + (boxH - refH) / 2;
    const int x = left + ow - ink.left;
    const int y = top + ow - ref.top;

    HGDIOBJ oldFont = SelectObject(hdc, PixelFont(size));
    int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    COLORREF oldColor = SetTextColor(hdc, outline);
    const int len = static_cast<int>(text.size());
    for (int dy = -ow; dy <= ow; ++dy)
        for (int dx = -ow; dx <= ow; ++dx)
            if (dx || dy) TextOutW(hdc, x + dx, y + dy, text.c_str(), len);
    SetTextColor(hdc, color);
    TextOutW(hdc, x, y, text.c_str(), len);
    SetTextColor(hdc, oldColor);
    SetBkMode(hdc, oldBkMode);
    SelectObject(hdc, oldFont);
}

void Gfx::BeginScaledText(HDC canvas, double scaleX, double scaleY)
{
    sDeferDC = canvas;
    sScaleX = scaleX;
    sScaleY = scaleY;
    sPending.clear();
}

void Gfx::EndScaledText(HDC screen)
{
    sDeferDC = nullptr;
    for (const PendingText& t : sPending) {
        RECT box = {
            static_cast<LONG>(t.box.left * sScaleX), static_cast<LONG>(t.box.top * sScaleY),
            static_cast<LONG>(t.box.right * sScaleX), static_cast<LONG>(t.box.bottom * sScaleY) };
        // 글씨 크기는 세로 배율만큼 (가로까지 늘리면 넓적해짐)
        int size = static_cast<int>(t.size * sScaleY + 0.5);
        DrawPixelText(screen, box, t.text, t.color, t.align, size, t.outline, t.outlineWidth);
    }
    sPending.clear();
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
    HGDIOBJ oldFont = SelectObject(hdc, SmallFont());
    int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    COLORREF oldColor = SetTextColor(hdc, RGB(0, 0, 0));
    // 그림자 먼저 찍기
    TextOutW(hdc, x + 1, y + 1, text.c_str(), static_cast<int>(text.size()));
    SetTextColor(hdc, color);
    TextOutW(hdc, x, y, text.c_str(), static_cast<int>(text.size()));

    SetTextColor(hdc, oldColor);
    SetBkMode(hdc, oldBkMode);
    SelectObject(hdc, oldFont);
}

void Gfx::DrawNoticeBar(HDC hdc, const std::wstring& text)
{
    // 가운데 검은 띠
    RECT band = { 0, Config::ScreenHeight / 2 - 40, Config::ScreenWidth, Config::ScreenHeight / 2 + 40 };
    FillRect(hdc, &band, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

    HGDIOBJ oldFont = SelectObject(hdc, SmallFont());
    int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    COLORREF oldColor = SetTextColor(hdc, RGB(255, 220, 120));
    std::wstring copy = text;
    DrawTextW(hdc, copy.data(), static_cast<int>(copy.size()), &band, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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
    if (sTintPixel) DeleteObject(sTintPixel);
    for (auto& kv : sPixelFonts) if (kv.second) DeleteObject(kv.second);
    sPixelFonts.clear();
    sInkCache.clear();
    if (sFontAdded) RemoveFontResourceExW(sFontPath.c_str(), FR_PRIVATE, nullptr);
    sFontAdded = false;
    sFontPath.clear();
    sTintPixel = nullptr;
    sTintBits = nullptr;
    sMemDC = nullptr;
    sTextFont = nullptr;
    sSmallFont = nullptr;
    sTextPen = nullptr;
    sTextBrush = nullptr;
    sBlackPixel = nullptr;
}
