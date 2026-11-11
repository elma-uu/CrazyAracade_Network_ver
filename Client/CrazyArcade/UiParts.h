#pragma once
#include "Common.h"
#include "Gfx.h"
#include "protocol.h"
#include "GameClient.h"

// 로비·대기방이 같이 쓰는 그리기 조각.
// 두 화면 모두 원작 UI 크기(800x600) 캔버스에 그린 뒤 전체 화면으로 늘린다.
// 좌표는 전부 배경 비트맵 픽셀 기준 (배경에서 직접 잰 값)
namespace Ui {
    constexpr int CanvasW = 800;
    constexpr int CanvasH = 600;

    // 왼쪽 칸 8개 (2줄 x 4칸): 칸 왼쪽 테두리 x / 이름 칸 오른쪽 테두리 x
    constexpr int SlotL[2][4] = { { 27, 132, 239, 345 }, { 27, 133, 239, 345 } };
    constexpr int SlotR[2][4] = { { 123, 228, 335, 441 }, { 123, 229, 335, 441 } };
    constexpr int RowStep = 145;
    constexpr int TileTop = 93;        // 칸 그림 101x101
    constexpr int NameTop = 198;       // 이름 칸 안쪽 18px
    constexpr int StatusTop = 217;     // 상태(준비) 칸 안쪽 16px

    // 글씨 색 (5가지만 쓴다)
    constexpr COLORREF White = RGB(255, 255, 255);
    constexpr COLORREF Label = RGB(0x5f, 0xf0, 0xff);
    constexpr COLORREF Yellow = RGB(0xff, 0xe1, 0x4d);
    constexpr COLORREF Pink = RGB(0xff, 0x9e, 0xc0);
    constexpr COLORREF Dim = RGB(0x9f, 0xd0, 0xf0);
    // 테두리: 마우스가 올라감 / 골라 둠
    constexpr COLORREF HoverFrame = RGB(0xff, 0xe1, 0x4d);
    constexpr COLORREF SelectFrame = RGB(0x9f, 0xe8, 0xff);

    // lobby_sprites.bmp 안의 위치 {x, y, w, h}
    constexpr RECT RandomIcon = { 0, 0, 49, 60 };
    constexpr RECT CrownSmall = { 52, 0, 19, 15 };
    constexpr RECT CrownTiny = { 74, 0, 17, 13 };

    inline RECT Box(int x, int y, int w, int h) { return { x, y, x + w, y + h }; }
    // 화면(1500x900) 마우스 좌표 → 캔버스 좌표
    POINT ToCanvas(POINT screen);

    // 노란 숫자(방 번호, 높이 12) / 초록 숫자(인원, 높이 10, '/' 포함)
    enum class DigitStyle { Yellow, Green };
    int DigitsWidth(DigitStyle style, const std::wstring& s);
    void DrawDigits(HDC c, DigitStyle style, const std::wstring& s, int x, int y);

    // {x, y, w, h} 영역의 알파 스프라이트
    void DrawSprite(HDC c, const RECT& r, int x, int y, BitmapID atlas = BitmapID::LobbySprites);

    enum class MapIconSize { Big = 64, Mid = 58, Small = 44 };
    void DrawMapIcon(HDC c, MAP_ID map, int x, int y, MapIconSize size);
    const wchar_t* MapName(MAP_ID map);
    std::wstring RoomNumber(int id);   // 3 → "003"

    // 정보 줄: '항목 : 값' (콜론과 값의 x를 고정해서 줄마다 세로로 맞춘다). y = 줄 안쪽 윗변, 높이 16
    void DrawKeyValue(HDC c, const wchar_t* key, const std::wstring& value,
        int x, int y, int colonX, int valueX, int right);

    // 왼쪽 아래 채팅 칸: 5줄씩 보이고, 오른쪽 스크롤바(화살표·손잡이 끌기·빈 곳 클릭)로 지난 내역을 본다.
    // 긴 줄은 칸 폭에 맞춰 자동으로 줄바꿈. 맨 아래를 보고 있으면 새 줄이 오면 따라 내려간다
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
        int mLastCount = 0;       // 지난 프레임 줄 수 (새 줄이 오면 보던 위치 유지)
        bool mDragging = false;
        int mDragOffset = 0;
    };

    // 채팅 입력칸 (아래 긴 칸 + 오른쪽 보내기 버튼). 칸을 누르면 입력, Enter·보내기 버튼으로 보낸다.
    // 입력 중에는 한글 IME를 켜고, 칸 밖을 누르거나 ESC면 입력을 끝낸다
    class ChatInput {
    public:
        // 보낼 글이 생긴 프레임에 true + out에 담는다
        bool Update(POINT mouse, bool mouseInClient, std::wstring& out);
        bool Focused() const { return mFocused; }
        void SetFocus(bool on);
        void Draw(HDC c) const;
    private:
        std::wstring mText;
        bool mFocused = false;
    };

    // 800x600 캔버스: Begin으로 그리고 Present로 화면 전체에 늘려 찍는다
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
