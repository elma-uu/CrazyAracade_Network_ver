#include "UiParts.h"
#include "Input.h"
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

namespace {
    // 채팅 칸 (lobby_bg / room_bg 기준)
    constexpr int kChatX = 30, kChatRight = 422;
    constexpr int kChatTop = 414, kChatLineH = 19, kChatRows = 5;
    const RECT kScrollUp = { 425, 412, 444, 429 };
    const RECT kScrollDown = { 425, 495, 444, 513 };
    constexpr int kThumbX = 426, kThumbSize = 17;
    constexpr int kThumbTop = 429, kThumbBottom = 477;   // 손잡이 윗변이 움직이는 범위
    const RECT kTrack = { 426, 429, 443, 495 };
}

std::vector<Ui::ChatView::VisualLine> Ui::ChatView::Wrap(const std::vector<GameClient::ChatLine>& lines)
{
    std::vector<VisualLine> out;
    const int maxW = kChatRight - kChatX - 4;   // 테두리 여유
    for (const auto& line : lines) {
        std::wstring rest = line.text;
        while (!rest.empty()) {
            int fit = std::max(1, Gfx::PixelTextFit(rest, maxW));
            out.push_back({ rest.substr(0, fit), line.color });
            rest.erase(0, fit);
        }
    }
    return out;
}

int Ui::ChatView::MaxScroll(int visualCount) const
{
    return std::max(0, visualCount - kChatRows);
}

int Ui::ChatView::ThumbY(int visualCount) const
{
    int maxScroll = MaxScroll(visualCount);
    if (maxScroll == 0) return kThumbBottom;   // 넘치지 않으면 맨 아래 (최신)
    return kThumbBottom - (kThumbBottom - kThumbTop) * mScroll / maxScroll;
}

void Ui::ChatView::Update(const std::vector<GameClient::ChatLine>& lines, POINT p, bool inClient)
{
    const int count = static_cast<int>(Wrap(lines).size());
    // 위로 올려 보고 있을 때 새 줄이 오면 보던 줄이 그대로 보이게
    if (mScroll > 0 && count > mLastCount) mScroll += count - mLastCount;
    mLastCount = count;
    const int maxScroll = MaxScroll(count);
    mScroll = std::clamp(mScroll, 0, maxScroll);

    if (mDragging) {
        if (!Input::GetKey(VK_LBUTTON)) { mDragging = false; return; }
        if (maxScroll > 0) {
            int y = std::clamp(static_cast<int>(p.y) - mDragOffset, kThumbTop, kThumbBottom);
            mScroll = (kThumbBottom - y) * maxScroll / (kThumbBottom - kThumbTop);
        }
        return;
    }
    if (!Input::GetKeyDown(VK_LBUTTON) || !inClient) return;

    const int thumbY = ThumbY(count);
    RECT thumb = { kThumbX, thumbY, kThumbX + kThumbSize, thumbY + kThumbSize };
    if (PtInRect(&kScrollUp, p)) mScroll = std::min(mScroll + 1, maxScroll);
    else if (PtInRect(&kScrollDown, p)) mScroll = std::max(mScroll - 1, 0);
    else if (PtInRect(&thumb, p)) { mDragging = true; mDragOffset = p.y - thumbY; }
    else if (PtInRect(&kTrack, p)) {
        // 손잡이 위 빈 곳 = 한 화면 위로, 아래 = 한 화면 아래로
        if (p.y < thumbY) mScroll = std::min(mScroll + kChatRows, maxScroll);
        else mScroll = std::max(mScroll - kChatRows, 0);
    }
}

void Ui::ChatView::Draw(HDC c, const std::vector<GameClient::ChatLine>& lines, const wchar_t* title) const
{
    Gfx::DrawPixelText(c, { 22, 389, 424, 409 }, title, Label);

    const std::vector<VisualLine> visual = Wrap(lines);
    const int count = static_cast<int>(visual.size());
    const int scroll = std::clamp(mScroll, 0, MaxScroll(count));
    const int first = std::max(0, count - kChatRows - scroll);
    for (int k = 0; k < kChatRows && first + k < count; ++k) {
        int y = kChatTop + k * kChatLineH;
        const VisualLine& v = visual[first + k];
        Gfx::DrawPixelText(c, { kChatX, y, kChatRight, y + kChatLineH }, v.text, v.color, Gfx::Align::Left);
    }
    // 스크롤 손잡이 (lobby_tiles.bmp 202,0)
    Gfx::DrawOpaque(c, BitmapID::LobbyTiles, kThumbX, ThumbY(count), kThumbSize, kThumbSize, 202, 0, kThumbSize, kThumbSize);
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
