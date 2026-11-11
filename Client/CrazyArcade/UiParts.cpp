#include "UiParts.h"
#include <cstdio>

namespace {
    void DigitGlyph(Ui::DigitStyle style, wchar_t ch, int& sx, int& sy, int& w, int& h, int& advance, int& yOffset)
    {
        yOffset = 0;
        if (style == Ui::DigitStyle::Yellow) {
            int d = ch - L'0';
            sx = d * 12; sy = 64; h = 12;
            w = (ch == L'1') ? 3 : 10;
            advance = (ch == L'1') ? 4 : 11;
        } else if (ch == L'/') {
            sx = 100; sy = 80; w = 5; h = 12; advance = 6; yOffset = -1;
        } else {
            int d = ch - L'0';
            sx = d * 10; sy = 80; h = 10;
            w = (ch == L'1') ? 3 : 7;
            advance = (ch == L'1') ? 4 : 8;
        }
    }
}

POINT Ui::ToCanvas(POINT screen)
{
    return { screen.x * CanvasW / Config::ScreenWidth, screen.y * CanvasH / Config::ScreenHeight };
}

int Ui::DigitsWidth(DigitStyle style, const std::wstring& s)
{
    int total = 0, last = 0;
    for (wchar_t ch : s) {
        int sx, sy, w, h, adv, yo;
        DigitGlyph(style, ch, sx, sy, w, h, adv, yo);
        total += adv; last = adv - w;
    }
    return total - last;
}

void Ui::DrawDigits(HDC c, DigitStyle style, const std::wstring& s, int x, int y)
{
    for (wchar_t ch : s) {
        int sx, sy, w, h, adv, yo;
        DigitGlyph(style, ch, sx, sy, w, h, adv, yo);
        Gfx::DrawAlpha(c, BitmapID::LobbySprites, x, y + yo, sx, sy, w, h);
        x += adv;
    }
}

void Ui::DrawSprite(HDC c, const RECT& r, int x, int y, BitmapID atlas)
{
    Gfx::DrawAlpha(c, atlas, x, y, r.left, r.top, r.right, r.bottom);
}

void Ui::DrawMapIcon(HDC c, MAP_ID map, int x, int y, MapIconSize size)
{
    int index = std::clamp(static_cast<int>(map) - 1, 0, 2);
    int s = static_cast<int>(size);
    int srcY = (size == MapIconSize::Big) ? 0 : (size == MapIconSize::Mid) ? 64 : 122;
    Gfx::DrawOpaque(c, BitmapID::LobbyMaps, x, y, s, s, index * 64, srcY, s, s);
}

const wchar_t* Ui::MapName(MAP_ID map)
{
    switch (map) {
    case MAP_CITY: return L"도시";
    case MAP_ICE:  return L"얼음";
    default:       return L"숲";
    }
}

std::wstring Ui::RoomNumber(int id)
{
    wchar_t buf[8];
    swprintf_s(buf, L"%03d", id);
    return buf;
}

void Ui::DrawKeyValue(HDC c, const wchar_t* key, const std::wstring& value,
    int x, int y, int colonX, int valueX, int right)
{
    Gfx::DrawPixelText(c, { x, y, colonX, y + 16 }, key, Label, Gfx::Align::Left);
    Gfx::DrawPixelText(c, { colonX, y, colonX + 8, y + 16 }, L":", Label, Gfx::Align::Left);
    Gfx::DrawPixelText(c, { valueX, y, right, y + 16 }, value, White, Gfx::Align::Left);
}

void Ui::ChatLog::Add(const std::wstring& text, COLORREF color)
{
    mLines.push_back({ text, color });
    constexpr size_t kMaxLines = 5;
    if (mLines.size() > kMaxLines) mLines.erase(mLines.begin());
}

void Ui::ChatLog::Draw(HDC c) const
{
    for (size_t k = 0; k < mLines.size(); ++k) {
        int y = 414 + static_cast<int>(k) * 19;
        Gfx::DrawPixelText(c, { 30, y, 420, y + 19 }, mLines[k].text, mLines[k].color, Gfx::Align::Left);
    }
}

Ui::Canvas::~Canvas()
{
    if (mDC) {
        SelectObject(mDC, mOld);
        DeleteDC(mDC);
    }
    if (mBitmap) DeleteObject(mBitmap);
}

HDC Ui::Canvas::Begin(HDC screen)
{
    if (!mDC) {
        mDC = CreateCompatibleDC(screen);
        mBitmap = CreateCompatibleBitmap(screen, CanvasW, CanvasH);
        mOld = SelectObject(mDC, mBitmap);
    }
    return mDC;
}

void Ui::Canvas::Present(HDC screen) const
{
    // 픽셀 느낌이 뭉개지지 않도록 가장 가까운 픽셀로 늘린다
    int oldMode = SetStretchBltMode(screen, COLORONCOLOR);
    StretchBlt(screen, 0, 0, Config::ScreenWidth, Config::ScreenHeight, mDC, 0, 0, CanvasW, CanvasH, SRCCOPY);
    SetStretchBltMode(screen, oldMode);
}
