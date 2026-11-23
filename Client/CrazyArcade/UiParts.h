#pragma once
#include "Common.h"
#include "Gfx.h"
#include "protocol.h"
#include "GameClient.h"

// 로비와 대기방이 같이 쓰는 UI
namespace Ui {
    constexpr int CanvasW = 800;
    constexpr int CanvasH = 600;

    // 방 칸 x 좌표
    constexpr int SlotL[2][4] = { { 27, 132, 239, 345 }, { 27, 133, 239, 345 } };
    constexpr int SlotR[2][4] = { { 123, 228, 335, 441 }, { 123, 229, 335, 441 } };
    constexpr int RowStep = 145;
    constexpr int TileTop = 93;        // 칸 그림 101x101
    constexpr int NameTop = 198;       // 이름 칸 안쪽 18px
    constexpr int StatusTop = 217;     // 상태(준비) 칸 안쪽 16px

    // 글씨 색
    constexpr COLORREF White = RGB(255, 255, 255);
    constexpr COLORREF Label = RGB(0x5f, 0xf0, 0xff);
    constexpr COLORREF Yellow = RGB(0xff, 0xe1, 0x4d);
    constexpr COLORREF Pink = RGB(0xff, 0x9e, 0xc0);
    constexpr COLORREF Dim = RGB(0x9f, 0xd0, 0xf0);
    // 테두리 색
    constexpr COLORREF HoverFrame = RGB(0xff, 0xe1, 0x4d);
    constexpr COLORREF SelectFrame = RGB(0x9f, 0xe8, 0xff);

    // lobby_sprites.bmp 안의 위치 {x, y, w, h}
    constexpr RECT RandomIcon = { 0, 0, 49, 60 };
    constexpr RECT CrownSmall = { 52, 0, 19, 15 };
    constexpr RECT CrownTiny = { 74, 0, 17, 13 };

    inline RECT Box(int x, int y, int w, int h) { return { x, y, x + w, y + h }; }
    // 캔버스 좌표로 변환
    POINT ToCanvas(POINT screen);

    // 숫자 그림 종류
    enum class DigitStyle { Yellow, Green };
    int DigitsWidth(DigitStyle style, const std::wstring& s);
    void DrawDigits(HDC c, DigitStyle style, const std::wstring& s, int x, int y);

    // {x, y, w, h} 영역의 알파 스프라이트
    void DrawSprite(HDC c, const RECT& r, int x, int y, BitmapID atlas = BitmapID::LobbySprites);

    enum class MapIconSize { Big = 64, Mid = 58, Small = 44 };
    void DrawMapIcon(HDC c, MAP_ID map, int x, int y, MapIconSize size);
    const wchar_t* MapName(MAP_ID map);
    std::wstring RoomNumber(int id);   // 3 → "003"

    // '항목 : 값' 한 줄
    void DrawKeyValue(HDC c, const wchar_t* key, const std::wstring& value,
        int x, int y, int colonX, int valueX, int right);

    // 채팅 내역 칸
    class ChatView {
    public:
        void Update(const std::vector<GameClient::ChatLine>& lines, POINT mouse, bool mouseInClient);
        void Draw(HDC c, const std::vector<GameClient::ChatLine>& lines, const wchar_t* title) const;
    private:
        struct VisualLine { std::wstring text; COLORREF color; };
        static std::vector<VisualLine> Wrap(const std::vector<GameClient::ChatLine>& lines);
        int MaxScroll(int visualCount) const;
        int ThumbY(int visualCount) const;

        int mScroll = 0;          // 맨 아래에서 몇 줄 올라가 있는지
        int mLastCount = 0;       // 지난 프레임 줄 수
        bool mDragging = false;
        int mDragOffset = 0;
    };

    // 채팅 입력칸
    class ChatInput {
    public:
        // 보낼 글이 생기면 true
        bool Update(POINT mouse, bool mouseInClient, std::wstring& out);
        bool Focused() const { return mFocused; }
        void SetFocus(bool on);
        void Draw(HDC c) const;
    private:
        std::wstring mText;
        bool mFocused = false;
    };

    // 800x600으로 그린 뒤 화면에 늘린다
    class Canvas {
    public:
        ~Canvas();
        HDC Begin(HDC screen);
        void Present(HDC screen) const;
    private:
        HDC mDC = nullptr;
        HBITMAP mBitmap = nullptr;
        HGDIOBJ mOld = nullptr;
    };
}
