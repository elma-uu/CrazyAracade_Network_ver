#include "LobbyScene.h"
#include "SceneManager.h"
#include "NetworkManager.h"
#include "GameClient.h"
#include "Input.h"
#include "Gfx.h"
#include <cstdio>

namespace {
    // 캔버스 = 원작 UI 크기. 좌표는 모두 lobby_bg.bmp 픽셀 기준 (배경에서 직접 잰 값)
    constexpr int kCanvasW = 800;
    constexpr int kCanvasH = 600;

    // 방 칸 8개 (2줄 x 4칸): 칸 왼쪽 테두리 x / 이름 칸 오른쪽 테두리 x
    constexpr int kSlotL[2][4] = { { 27, 132, 239, 345 }, { 27, 133, 239, 345 } };
    constexpr int kSlotR[2][4] = { { 123, 228, 335, 441 }, { 123, 229, 335, 441 } };
    constexpr int kRowStep = 145;
    constexpr int kTileTop = 93;        // 칸 그림 101x101
    constexpr int kNameTop = 198;       // 이름 칸 안쪽 18px
    constexpr int kStatusTop = 217;     // 상태 칸 안쪽 16px

    // 글씨 색 (5가지만 쓴다)
    constexpr COLORREF kWhite = RGB(255, 255, 255);
    constexpr COLORREF kLabel = RGB(0x5f, 0xf0, 0xff);
    constexpr COLORREF kYellow = RGB(0xff, 0xe1, 0x4d);
    constexpr COLORREF kPink = RGB(0xff, 0x9e, 0xc0);
    constexpr COLORREF kDim = RGB(0x9f, 0xd0, 0xf0);

    // lobby_sprites.bmp 안의 위치
    constexpr RECT kRandomIcon = { 0, 0, 49, 60 };
    constexpr RECT kCrownSmall = { 52, 0, 19, 15 };    // {x, y, w, h}
    constexpr RECT kCrownTiny = { 74, 0, 17, 13 };

    enum class DigitStyle { Yellow, Green };

    RECT Box(int x, int y, int w, int h) { return { x, y, x + w, y + h }; }

    // 노란 숫자(방 번호, 높이 12) / 초록 숫자(인원, 높이 10). '/'는 초록에만 있다
    void DigitGlyph(DigitStyle style, wchar_t ch, int& sx, int& sy, int& w, int& h, int& advance, int& yOffset)
    {
        yOffset = 0;
        if (style == DigitStyle::Yellow) {
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

    int DigitsWidth(DigitStyle style, const std::wstring& s)
    {
        int total = 0, last = 0;
        for (wchar_t ch : s) {
            int sx, sy, w, h, adv, yo;
            DigitGlyph(style, ch, sx, sy, w, h, adv, yo);
            total += adv; last = adv - w;
        }
        return total - last;
    }

    void DrawDigits(HDC c, DigitStyle style, const std::wstring& s, int x, int y)
    {
        for (wchar_t ch : s) {
            int sx, sy, w, h, adv, yo;
            DigitGlyph(style, ch, sx, sy, w, h, adv, yo);
            Gfx::DrawAlpha(c, BitmapID::LobbySprites, x, y + yo, sx, sy, w, h);
            x += adv;
        }
    }

    void DrawSprite(HDC c, const RECT& r, int x, int y)
    {
        Gfx::DrawAlpha(c, BitmapID::LobbySprites, x, y, r.left, r.top, r.right, r.bottom);
    }

    // 맵 아이콘: big = 64px(방 칸), 아니면 58px(선택한 방 미리보기)
    void DrawMapIcon(HDC c, MAP_ID map, int x, int y, bool big)
    {
        int index = std::clamp(static_cast<int>(map) - 1, 0, 2);
        int size = big ? 64 : 58;
        Gfx::DrawOpaque(c, BitmapID::LobbyMaps, x, y, size, size, index * 64, big ? 0 : 64, size, size);
    }

    const wchar_t* MapName(MAP_ID map)
    {
        switch (map) {
        case MAP_CITY: return L"도시";
        case MAP_ICE:  return L"얼음";
        default:       return L"숲";
        }
    }

    std::wstring RoomNumber(int id)
    {
        wchar_t buf[8];
        swprintf_s(buf, L"%03d", id);
        return buf;
    }

    bool IsEmpty(const GameClient::RoomView& r) { return r.state == ROOM_EMPTY || r.players == 0; }
    bool CanEnter(const GameClient::RoomView& r)
    {
        return IsEmpty(r) || (r.state == ROOM_WAITING && r.players < MAX_ROOM_PLAYER);
    }

    // 정보 줄: '항목 : 값' (콜론과 값의 x를 고정해서 줄마다 세로로 맞춘다)
    void DrawKeyValue(HDC c, const wchar_t* key, const std::wstring& value,
        int x, int y, int colonX, int valueX, int right)
    {
        Gfx::DrawPixelText(c, { x, y, colonX, y + 16 }, key, kLabel, Gfx::Align::Left);
        Gfx::DrawPixelText(c, { colonX, y, colonX + 8, y + 16 }, L":", kLabel, Gfx::Align::Left);
        Gfx::DrawPixelText(c, { valueX, y, right, y + 16 }, value, kWhite, Gfx::Align::Left);
    }
}

LobbyScene::~LobbyScene()
{
    if (mCanvasDC) {
        SelectObject(mCanvasDC, mOldCanvas);
        DeleteDC(mCanvasDC);
    }
    if (mCanvas) DeleteObject(mCanvas);
}

void LobbyScene::OnEnter()
{
    if (!GameClient::IsLobbyPreview()) {
        GameClient::ClearRoomList();
        GameClient::RequestRoomList();
    }
    AddLog(L"[알림] 로비에 들어왔습니다.", kYellow);
    if (GameClient::IsLobbyPreview()) AddLog(L"[알림] 미리보기: 서버 없이 샘플 방 목록을 보여 줍니다.", kDim);
}

void LobbyScene::Update(float)
{
    if (Input::GetKeyDown(VK_ESCAPE)) {
        if (GameClient::IsLobbyPreview()) {
            GameClient::EndLobbyPreview();
            GameClient::ResetLogin();
            SceneManager::ChangeScene(SceneType::Login);
            return;
        }
        NetworkManager::Disconnect();
        GameClient::ResetLogin();
        SceneManager::ChangeScene(SceneType::Title);
        return;
    }
    // 서버와 끊기면 로그인 화면으로 (끊김 안내는 GameClient가 띄운다)
    if (!GameClient::IsLobbyPreview() && !NetworkManager::IsConnected()) {
        SceneManager::ChangeScene(SceneType::Login);
        return;
    }

    // (F5는 에코 테스트 접속 키라 새로고침에 쓰지 않는다)
    if (Input::GetKeyDown(VK_RETURN)) TryEnter();
    if (!Input::GetKeyDown(VK_LBUTTON) || !Input::IsMouseInClient()) return;

    // 화면(1500x900) 좌표 → 캔버스(800x600) 좌표
    POINT m = Input::GetMousePosition();
    POINT p = { m.x * kCanvasW / Config::ScreenWidth, m.y * kCanvasH / Config::ScreenHeight };

    for (int i = 0; i < MAX_ROOM; ++i) {
        RECT rc = RoomRect(i);
        if (PtInRect(&rc, p)) { SelectRoom(i); return; }
    }
    RECT refresh = Box(345, 62, 98, 20);
    RECT quick = Box(647, 429, 122, 45);
    RECT enter = Box(514, 495, 188, 53);
    if (PtInRect(&refresh, p)) Refresh();
    else if (PtInRect(&quick, p)) QuickEnter();
    else if (PtInRect(&enter, p)) TryEnter();
}

RECT LobbyScene::RoomRect(int index)
{
    const int row = index / 4, col = index % 4;
    const int top = kTileTop + kRowStep * row;
    // 칸 그림 + 이름 칸 + 상태 칸 전체
    return { kSlotL[row][col] - 1, top, kSlotR[row][col] + 2, kStatusTop + kRowStep * row + 16 };
}

void LobbyScene::SelectRoom(int index)
{
    mSelected = index;
    const GameClient::RoomView& r = GameClient::Room(index);
    if (CanEnter(r)) AddLog(L"[알림] " + RoomNumber(r.id) + L"번 방을 골랐습니다. 입장을 누르세요.", kYellow);
    else AddLog(L"[알림] " + RoomNumber(r.id) + L"번 방은 " + (r.state == ROOM_PLAYING ? L"게임 중" : L"꽉 차서") +
        L" 들어갈 수 없어요.", kPink);
}

void LobbyScene::Refresh()
{
    if (!GameClient::IsLobbyPreview()) GameClient::RequestRoomList();
    AddLog(L"[알림] 방 목록을 새로 받았습니다.", kYellow);
}

void LobbyScene::QuickEnter()
{
    // 사람이 있는 대기 방 먼저, 없으면 빈 방
    int pick = -1;
    for (int i = 0; i < MAX_ROOM && pick < 0; ++i) {
        const GameClient::RoomView& r = GameClient::Room(i);
        if (!IsEmpty(r) && CanEnter(r)) pick = i;
    }
    for (int i = 0; i < MAX_ROOM && pick < 0; ++i)
        if (IsEmpty(GameClient::Room(i))) pick = i;

    if (pick < 0) { AddLog(L"[알림] 들어갈 수 있는 방이 없어요.", kPink); return; }
    SelectRoom(pick);
    TryEnter();
}

void LobbyScene::TryEnter()
{
    if (mSelected < 0) { AddLog(L"[알림] 먼저 방을 골라 주세요.", kPink); return; }
    const GameClient::RoomView& r = GameClient::Room(mSelected);
    if (!CanEnter(r)) { AddLog(L"[알림] 이 방에는 들어갈 수 없어요.", kPink); return; }
    // TODO: C2S_JoinRoom / S2C_RoomEnterResult (형민 담당 패킷)가 들어오면 여기서 보낸다
    GameClient::ShowNotice(L"방 입장은 C2S_JoinRoom 연결 후에 됩니다");
}

void LobbyScene::AddLog(const std::wstring& text, COLORREF color)
{
    mLog.push_back({ text, color });
    constexpr size_t kMaxLines = 5;
    if (mLog.size() > kMaxLines) mLog.erase(mLog.begin());
}

void LobbyScene::Render(HDC hdc)
{
    if (!mCanvasDC) {
        mCanvasDC = CreateCompatibleDC(hdc);
        mCanvas = CreateCompatibleBitmap(hdc, kCanvasW, kCanvasH);
        mOldCanvas = SelectObject(mCanvasDC, mCanvas);
    }
    HDC c = mCanvasDC;

    Gfx::DrawOpaque(c, BitmapID::LobbyBg, 0, 0, kCanvasW, kCanvasH, 0, 0, kCanvasW, kCanvasH);
    DrawRooms(c);
    DrawHeader(c);
    DrawProfile(c);
    DrawSelectedRoom(c);
    DrawButtons(c);
    DrawChat(c);
    Gfx::DrawPixelText(c, Box(32, 3, 400, 23), L"[공지] 게임 중이거나 꽉 찬 방(X 칸)은 들어갈 수 없어요",
        kYellow, Gfx::Align::Left);

    // 픽셀 느낌이 뭉개지지 않도록 가장 가까운 픽셀로 늘린다
    int oldMode = SetStretchBltMode(hdc, COLORONCOLOR);
    StretchBlt(hdc, 0, 0, Config::ScreenWidth, Config::ScreenHeight, c, 0, 0, kCanvasW, kCanvasH, SRCCOPY);
    SetStretchBltMode(hdc, oldMode);
}

void LobbyScene::DrawRooms(HDC c) const
{
    for (int i = 0; i < MAX_ROOM; ++i) {
        const int row = i / 4, col = i % 4;
        const int L = kSlotL[row][col], R = kSlotR[row][col];
        const int tileX = L - 1, tileY = kTileTop + kRowStep * row;
        const int barX = L + 2, barW = R - L - 2;
        const int nameY = kNameTop + kRowStep * row, statusY = kStatusTop + kRowStep * row;

        const GameClient::RoomView& r = GameClient::Room(i);
        const bool empty = IsEmpty(r);
        const bool canEnter = CanEnter(r);

        // 들어갈 수 있는 방 = 열린 칸, 게임 중이거나 꽉 찬 방 = X 칸
        Gfx::DrawOpaque(c, BitmapID::LobbyTiles, tileX, tileY, 101, 101, canEnter ? 0 : 101, 0, 101, 101);
        if (empty) {
            DrawSprite(c, kRandomIcon, tileX + 26, tileY + 20);
        } else {
            DrawMapIcon(c, r.map, tileX + 19, tileY + 22, true);
            if (!canEnter) Gfx::FillAlpha(c, Box(tileX + 19, tileY + 22, 64, 64), RGB(6, 32, 74), 120);
        }
        DrawDigits(c, DigitStyle::Yellow, RoomNumber(r.id), tileX + 7, tileY + 6);

        if (empty) {
            Gfx::DrawPixelText(c, Box(barX, nameY, barW, 18), L"빈 방", kDim);
            Gfx::DrawPixelText(c, Box(barX + 2, statusY, 56, 16), L"입장가능", kDim, Gfx::Align::Left);
        } else {
            DrawSprite(c, kCrownSmall, barX + 4, nameY + 1);
            Gfx::DrawPixelText(c, { barX + 25, nameY, barX + barW - 2, nameY + 18 }, r.host, kWhite, Gfx::Align::Left);
            const wchar_t* state = L"대기 중";
            COLORREF stateColor = kWhite;
            if (r.state == ROOM_PLAYING) { state = L"게임 중"; stateColor = kPink; }
            else if (r.players >= MAX_ROOM_PLAYER) { state = L"꽉 참"; stateColor = kPink; }
            Gfx::DrawPixelText(c, Box(barX + 2, statusY, 56, 16), state, stateColor, Gfx::Align::Left);
        }
        // 인원: 상태 칸 오른쪽
        std::wstring count = std::to_wstring(r.players) + L"/" + std::to_wstring(MAX_ROOM_PLAYER);
        DrawDigits(c, DigitStyle::Green, count, barX + barW - 3 - DigitsWidth(DigitStyle::Green, count), statusY + 3);

        if (i == mSelected)
            Gfx::DrawRoundFrame(c, Box(L - 3, tileY - 2, 104, 145), 8, RGB(0xff, 0xe1, 0x4d), 2);
    }
}

void LobbyScene::DrawHeader(HDC c) const
{
    // 왼쪽 번호 칸에는 고른 방 번호
    if (mSelected >= 0) DrawDigits(c, DigitStyle::Yellow, RoomNumber(GameClient::Room(mSelected).id), 31, 66);
    Gfx::DrawPixelText(c, Box(80, 59, 60, 26), L"방 목록", kWhite, Gfx::Align::Left);
    Gfx::DrawPixelText(c, Box(144, 59, 196, 26), L"1~8번 방 · 눌러서 고르기", kDim, Gfx::Align::Left);
    Gfx::DrawPixelText(c, Box(345, 62, 98, 20), L"새로고침", kWhite);
}

void LobbyScene::DrawProfile(HDC c) const
{
    Gfx::DrawPixelText(c, Box(496, 186, 120, 22), L"내 정보", kLabel, Gfx::Align::Left);

    const int wins = GameClient::Wins(), losses = GameClient::Losses();
    std::wstring record = std::to_wstring(wins) + L"승 " + std::to_wstring(losses) + L"패";
    std::wstring rate = (wins + losses > 0) ? std::to_wstring(wins * 100 / (wins + losses)) + L"%" : L"-";
    DrawKeyValue(c, L"닉네임", GameClient::MyName(), 504, 212, 554, 566, 760);
    DrawKeyValue(c, L"전적", record, 504, 236, 554, 566, 760);
    DrawKeyValue(c, L"승률", rate, 504, 260, 554, 566, 760);

    Gfx::DrawPixelText(c, Box(486, 292, 290, 24), L"로비에서는 캐릭터·색을 고르지 않아요", kDim);
}

void LobbyScene::DrawSelectedRoom(HDC c) const
{
    if (mSelected < 0) {
        Gfx::DrawPixelText(c, Box(646, 347, 126, 16), L"방을 골라 주세요", kLabel);
        DrawKeyValue(c, L"인원", L"-", 654, 366, 686, 696, 770);
        DrawKeyValue(c, L"방장", L"-", 654, 385, 686, 696, 770);
        DrawKeyValue(c, L"상태", L"-", 654, 404, 686, 696, 770);
        return;
    }

    const GameClient::RoomView& r = GameClient::Room(mSelected);
    const bool empty = IsEmpty(r);
    Gfx::DrawPixelText(c, Box(646, 347, 126, 16), RoomNumber(r.id) + L"번 방", kLabel);
    DrawKeyValue(c, L"인원", std::to_wstring(r.players) + L" / " + std::to_wstring(MAX_ROOM_PLAYER), 654, 366, 686, 696, 770);

    if (empty) {
        DrawSprite(c, kRandomIcon, 538, 367);
        Gfx::DrawPixelText(c, Box(520, 429, 86, 29), L"빈 방", kYellow);
        DrawKeyValue(c, L"방장", L"-", 654, 385, 686, 696, 770);
        DrawKeyValue(c, L"상태", L"입장 가능", 654, 404, 686, 696, 770);
        return;
    }

    DrawMapIcon(c, r.map, 534, 368, false);
    Gfx::DrawPixelText(c, Box(520, 429, 86, 29), MapName(r.map), kYellow);
    DrawKeyValue(c, L"방장", L"", 654, 385, 686, 696, 770);
    DrawSprite(c, kCrownTiny, 696, 387);
    Gfx::DrawPixelText(c, { 714, 385, 768, 401 }, r.host, kWhite, Gfx::Align::Left);
    const wchar_t* state = (r.state == ROOM_PLAYING) ? L"게임 중"
        : (r.players >= MAX_ROOM_PLAYER) ? L"꽉 참" : L"대기 중";
    DrawKeyValue(c, L"상태", state, 654, 404, 686, 696, 770);
}

void LobbyScene::DrawButtons(HDC c) const
{
    // 버튼 글씨는 원래 버튼 글씨 크기·테두리 색에 맞춘다
    Gfx::DrawPixelText(c, Box(647, 429, 122, 45), L"빠른 입장", kWhite, Gfx::Align::Center, 18, RGB(0x0b, 0x3f, 0x86), 2);
    Gfx::DrawPixelText(c, Box(514, 495, 188, 53), L"입     장", kWhite, Gfx::Align::Center, 24, RGB(0xc3, 0x4a, 0x00), 2);
}

void LobbyScene::DrawChat(HDC c) const
{
    for (size_t k = 0; k < mLog.size(); ++k) {
        int y = 414 + static_cast<int>(k) * 19;
        Gfx::DrawPixelText(c, { 30, y, 420, y + 19 }, mLog[k].text, mLog[k].color, Gfx::Align::Left);
    }
}
