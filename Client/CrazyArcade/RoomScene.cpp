#include "RoomScene.h"
#include "SceneManager.h"
#include "NetworkManager.h"
#include "GameClient.h"
#include "Input.h"
#include "Gfx.h"
#include "Game.h"

using namespace Ui;

namespace {
    // 안내 문구도 채팅 칸에 쌓는다
    void Log(const std::wstring& text, COLORREF color)
    {
        GameClient::AddChat(GameClient::ChatChannel::Room, text, color);
    }

    // room_bg.bmp 기준 좌표
    constexpr int kMapCellX = 496, kMapCellY = 200, kMapCellW = 84, kMapCellH = 70, kMapCellStep = 92;
    constexpr int kColorX = 487, kColorY = 284, kColorW = 36, kColorH = 35;   // 팔레트 한 칸
    // 배경 팔레트는 8칸이라 마지막 칸은 막아 둔다
    constexpr int kColorCount = 7;
    constexpr int kPaletteCells = 8;
    const RECT kLeaveButton = Box(647, 429, 122, 45);
    const RECT kStartButton = Box(514, 495, 188, 53);
    const RECT kPowerButton = Box(749, 570, 31, 27);

    // room_sprites.bmp 안의 위치 {x, y, w, h}
    RECT CharSprite(int color) { return { std::clamp(color, 0, kColorCount - 1) * 88, 0, 86, 86 }; }
    constexpr RECT kReadyCheck = { 0, 88, 27, 19 };   // 초록 체크 하나
    constexpr RECT kColorCheck = { 30, 88, 30, 21 };

    // 준비 안 함 글씨 색
    constexpr COLORREF kNotReady = RGB(0x3d, 0x7f, 0xc0);

    const MAP_ID kMaps[3] = { MAP_FOREST, MAP_CITY, MAP_ICE };
    const wchar_t* MapRule(MAP_ID map)
    {
        switch (map) {
        case MAP_CITY: return L"도시 (기차 조심)";
        case MAP_ICE:  return L"얼음 (하트 지키기)";
        default:       return L"숲 (사과 쟁탈전)";
        }
    }

    RECT MapCell(int k) { return Box(kMapCellX + k * kMapCellStep, kMapCellY, kMapCellW, kMapCellH); }
    RECT ColorCell(int i) { return Box(kColorX + i * kColorW, kColorY, kColorW, kColorH); }

    int PlayerCount(const GameClient::RoomState& room)
    {
        int n = 0;
        for (const auto& p : room.players) if (p.exist) ++n;
        return n;
    }
    // 방장을 뺀 사람 중 준비한 사람 수
    int ReadyCount(const GameClient::RoomState& room)
    {
        int n = 0;
        for (int i = 0; i < MAX_ROOM_PLAYER; ++i)
            if (i != room.hostSlot && room.players[i].exist && room.players[i].ready) ++n;
        return n;
    }
    bool CanStart(const GameClient::RoomState& room)
    {
        int players = PlayerCount(room);
        return players >= 2 && ReadyCount(room) == players - 1;
    }
    // 다른 사람이 쓰고 있는 색인지
    bool ColorTaken(const GameClient::RoomState& room, int color, int mySlot)
    {
        for (int i = 0; i < MAX_ROOM_PLAYER; ++i)
            if (i != mySlot && room.players[i].exist && room.players[i].color == color) return true;
        return false;
    }
}

void RoomScene::OnEnter()
{
    Log(L"[알림] " + RoomNumber(GameClient::CurrentRoom().roomId) + L"번 방에 들어왔습니다.", Yellow);
}

void RoomScene::OnExit()
{
    mInput.SetFocus(false);   // 한글 IME 끄기
}

void RoomScene::Leave()
{
    GameClient::LeaveRoom();
    SceneManager::ChangeScene(SceneType::Lobby);
}

void RoomScene::Update(float)
{
    if (!GameClient::IsLobbyPreview() && !NetworkManager::IsConnected()) {
        SceneManager::ChangeScene(SceneType::Login);
        return;
    }
    if (GameClient::GetJoinState() != GameClient::JoinState::Joined) {
        SceneManager::ChangeScene(SceneType::Lobby);
        return;
    }
    // 게임이 시작되면 그 맵으로
    if (GameClient::IsInGame()) {
        switch (GameClient::GameStart().map) {
        case MAP_CITY: SceneManager::ChangeScene(SceneType::City); break;
        case MAP_ICE:  SceneManager::ChangeScene(SceneType::Ice);  break;
        default:       SceneManager::ChangeScene(SceneType::Forest); break;
        }
        return;
    }
    if (Input::GetKeyDown(VK_ESCAPE)) {
        // 채팅 입력 중이면 입력만 끝낸다
        if (mInput.Focused()) mInput.SetFocus(false);
        else Leave();
        return;
    }

    // 마우스 올린 칸
    POINT p = ToCanvas(Input::GetMousePosition());
    const bool inClient = Input::IsMouseInClient();
    mHoverMap = mHoverColor = mHoverButton = -1;
    if (inClient) {
        for (int k = 0; k < 3; ++k) { RECT rc = MapCell(k); if (PtInRect(&rc, p)) mHoverMap = k; }
        for (int i = 0; i < kColorCount; ++i) { RECT rc = ColorCell(i); if (PtInRect(&rc, p)) mHoverColor = i; }
        if (PtInRect(&kLeaveButton, p)) mHoverButton = 0;
        if (PtInRect(&kStartButton, p)) mHoverButton = 1;
    }

    LogRoomChanges();
    mChat.Update(GameClient::Chat(GameClient::ChatChannel::Room), p, inClient);

    // 채팅 입력
    std::wstring chat;
    if (mInput.Update(p, inClient, chat)) GameClient::SendChat(GameClient::ChatChannel::Room, chat);
    if (Input::GetKeyDown(VK_RETURN) && !mInput.Focused()) PressStart();
    if (!Input::GetKeyDown(VK_LBUTTON) || !inClient) return;
    if (PtInRect(&kPowerButton, p)) { Game::Quit(); return; }
    if (mHoverButton == 0) { Leave(); return; }
    if (mHoverButton == 1) { PressStart(); return; }

    const GameClient::RoomState& room = GameClient::CurrentRoom();
    if (mHoverMap >= 0) {
        if (!GameClient::AmIHost()) Log(L"[알림] 맵은 방장만 바꿀 수 있어요.", Pink);
        else if (room.map != kMaps[mHoverMap]) GameClient::SelectMap(kMaps[mHoverMap]);
        return;
    }
    if (mHoverColor >= 0) {
        if (ColorTaken(room, mHoverColor, GameClient::MySlot())) Log(L"[알림] 다른 사람이 쓰고 있는 색이에요.", Pink);
        else GameClient::SelectColor(mHoverColor);
    }
}

void RoomScene::PressStart()
{
    const GameClient::RoomState& room = GameClient::CurrentRoom();
    if (GameClient::AmIHost()) {
        if (PlayerCount(room) < 2) { Log(L"[알림] 2명 이상 모여야 시작할 수 있어요.", Pink); return; }
        if (!CanStart(room)) { Log(L"[알림] 모두 준비해야 시작할 수 있어요.", Pink); return; }
        Log(L"[알림] 게임 시작을 요청했습니다.", Yellow);
        GameClient::StartGame();
        return;
    }
    const int mySlot = GameClient::MySlot();
    if (mySlot >= 0) GameClient::SetReady(!room.players[mySlot].ready);
}

// 방 상태 변화를 채팅 칸에 알린다
void RoomScene::LogRoomChanges()
{
    const GameClient::RoomState& now = GameClient::CurrentRoom();
    if (!mHasPrev) { mPrev = now; mHasPrev = true; return; }

    for (int i = 0; i < MAX_ROOM_PLAYER; ++i) {
        const auto& a = mPrev.players[i];
        const auto& b = now.players[i];
        if (!a.exist && b.exist) Log(L"[알림] " + b.name + L"님이 들어왔습니다.", Yellow);
        else if (a.exist && !b.exist) Log(L"[알림] " + a.name + L"님이 나갔습니다.", Yellow);
        else if (a.exist && b.exist && a.ready != b.ready && i != now.hostSlot)
            Log(L"[알림] " + b.name + (b.ready ? L"님이 준비했습니다." : L"님이 준비를 풀었습니다."), White);
    }
    if (mPrev.hostSlot != now.hostSlot && now.players[now.hostSlot].exist)
        Log(L"[알림] " + now.players[now.hostSlot].name + L"님이 방장이 되었습니다.", Yellow);
    if (mPrev.map != now.map)
        Log(std::wstring(L"[알림] 맵이 ") + MapName(now.map) + L"(으)로 바뀌었습니다. 모두 다시 준비해 주세요.", Yellow);
    mPrev = now;
}

void RoomScene::Render(HDC hdc)
{
    HDC c = mCanvas.Begin(hdc);
    Gfx::DrawOpaque(c, BitmapID::RoomBg, 0, 0, CanvasW, CanvasH, 0, 0, CanvasW, CanvasH);
    DrawHeader(c);
    DrawSlots(c);
    DrawMapSelect(c);
    DrawColors(c);
    DrawRoomInfo(c);
    DrawButtons(c);
    mChat.Draw(c, GameClient::Chat(GameClient::ChatChannel::Room), L"방 채팅");
    mInput.Draw(c);

    const wchar_t* notice = GameClient::AmIHost()
        ? L"[알림] 모두 준비하면 시작을 누르세요. 맵은 방장이 골라요"
        : L"[알림] 준비를 누르고 방장이 시작하기를 기다려요";
    Gfx::DrawPixelText(c, Box(32, 3, 400, 23), notice, Yellow, Gfx::Align::Left);
    mCanvas.Present(hdc);
}

void RoomScene::DrawHeader(HDC c) const
{
    DrawDigits(c, DigitStyle::Yellow, RoomNumber(GameClient::CurrentRoom().roomId), 31, 66);
    Gfx::DrawPixelText(c, Box(80, 59, 60, 26), L"대기방", White, Gfx::Align::Left);
    Gfx::DrawPixelText(c, Box(140, 59, 300, 26), L"방장이 맵을 고르고, 모두 준비하면 시작해요", Dim, Gfx::Align::Left);
}

void RoomScene::DrawSlots(HDC c) const
{
    const GameClient::RoomState& room = GameClient::CurrentRoom();
    for (int i = 0; i < MAX_ROOM_PLAYER; ++i) {
        const int L = SlotL[0][i], R = SlotR[0][i];
        const int tileX = L - 1, barX = L + 2, barW = R - L - 2;
        const GameClient::RoomPlayerView& pl = room.players[i];

        if (!pl.exist) {
            Gfx::DrawPixelText(c, Box(barX, NameTop, barW, 18), L"빈 자리", Dim);
            continue;
        }
        DrawSprite(c, CharSprite(pl.color), tileX + 8, TileTop + 6, BitmapID::RoomSprites);
        // 내 이름은 노랑
        Gfx::DrawPixelText(c, Box(barX, NameTop, barW, 18), pl.name, i == GameClient::MySlot() ? Yellow : White);

        if (i == room.hostSlot) {
            DrawSprite(c, CrownSmall, tileX + 78, TileTop + 5);
            Gfx::DrawPixelText(c, Box(barX, StatusTop, barW, 16), L"방   장", Pink);
        } else if (pl.ready) {
            DrawSprite(c, kReadyCheck, tileX + 70, TileTop + 6, BitmapID::RoomSprites);
            Gfx::DrawPixelText(c, Box(barX, StatusTop, barW, 16), L"준   비", Yellow);
        } else {
            Gfx::DrawPixelText(c, Box(barX, StatusTop, barW, 16), L"준   비", kNotReady);
        }
    }
}

void RoomScene::DrawMapSelect(HDC c) const
{
    const GameClient::RoomState& room = GameClient::CurrentRoom();
    Gfx::DrawPixelText(c, Box(496, 183, 120, 16), L"맵 선택", Label, Gfx::Align::Left);
    if (!GameClient::AmIHost())
        Gfx::DrawPixelText(c, Box(616, 183, 150, 16), L"방장만 바꿀 수 있어요", Dim, Gfx::Align::Right);

    for (int k = 0; k < 3; ++k) {
        RECT cell = MapCell(k);
        DrawMapIcon(c, kMaps[k], cell.left + 20, cell.top + 4, MapIconSize::Small);
        bool selected = room.map == kMaps[k];
        Gfx::DrawPixelText(c, Box(cell.left, cell.top + 50, kMapCellW, 16), MapName(kMaps[k]), selected ? Yellow : White);
        RECT frame = { cell.left - 2, cell.top - 2, cell.right + 2, cell.bottom + 2 };
        if (k == mHoverMap && GameClient::AmIHost()) Gfx::DrawRoundFrame(c, frame, 6, HoverFrame, 2);
        else if (selected) Gfx::DrawRoundFrame(c, frame, 6, SelectFrame, 2);
    }
}

void RoomScene::DrawColors(HDC c) const
{
    const GameClient::RoomState& room = GameClient::CurrentRoom();
    const int mySlot = GameClient::MySlot();
    const int myColor = (mySlot >= 0) ? room.players[mySlot].color : -1;

    for (int i = 0; i < kColorCount; ++i) {
        RECT cell = ColorCell(i);
        RECT inner = { cell.left + 3, cell.top + 2, cell.right - 4, cell.bottom - 3 };
        // 남이 쓰는 색은 X 표시
        if (ColorTaken(room, i, mySlot)) {
            Gfx::FillAlpha(c, inner, RGB(6, 32, 74), 150);
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
            HGDIOBJ old = SelectObject(c, pen);
            MoveToEx(c, inner.left + 8, inner.top + 8, nullptr); LineTo(c, inner.right - 8, inner.bottom - 8);
            MoveToEx(c, inner.right - 8, inner.top + 8, nullptr); LineTo(c, inner.left + 8, inner.bottom - 8);
            SelectObject(c, old);
            DeleteObject(pen);
        }
        if (i == myColor) DrawSprite(c, kColorCheck, cell.left + 3, cell.top - 4, BitmapID::RoomSprites);
        if (i == mHoverColor) Gfx::DrawRoundFrame(c, cell, 4, HoverFrame, 2);
    }
    for (int i = kColorCount; i < kPaletteCells; ++i) {
        RECT cell = ColorCell(i);
        Gfx::FillAlpha(c, { cell.left + 3, cell.top + 2, cell.right - 4, cell.bottom - 3 }, RGB(6, 32, 74), 200);
    }
}

void RoomScene::DrawRoomInfo(HDC c) const
{
    const GameClient::RoomState& room = GameClient::CurrentRoom();
    DrawMapIcon(c, room.map, 534, 368, MapIconSize::Mid);
    Gfx::DrawPixelText(c, Box(520, 429, 86, 29), MapName(room.map), Yellow);
    Gfx::DrawPixelText(c, Box(646, 347, 126, 16), MapRule(room.map), Label);

    const int players = PlayerCount(room);
    DrawKeyValue(c, L"인원", std::to_wstring(players) + L" / " + std::to_wstring(MAX_ROOM_PLAYER), 654, 366, 686, 696, 770);
    DrawKeyValue(c, L"방장", L"", 654, 385, 686, 696, 770);
    const auto& host = room.players[std::clamp(room.hostSlot, 0, MAX_ROOM_PLAYER - 1)];
    if (host.exist) {
        DrawSprite(c, CrownTiny, 696, 387);
        Gfx::DrawPixelText(c, { 714, 385, 768, 401 }, host.name, White, Gfx::Align::Left);
    }
    DrawKeyValue(c, L"준비", std::to_wstring(ReadyCount(room)) + L" / " + std::to_wstring(std::max(players - 1, 0)),
        654, 404, 686, 696, 770);
}

void RoomScene::DrawButtons(HDC c) const
{
    const GameClient::RoomState& room = GameClient::CurrentRoom();
    Gfx::DrawPixelText(c, kLeaveButton, L"방 나가기", White, Gfx::Align::Center, 18, RGB(0x0b, 0x3f, 0x86), 2);

    if (GameClient::AmIHost()) {
        // 아직 시작할 수 없으면 버튼과 글씨를 어둡게
        bool canStart = CanStart(room);
        if (!canStart) Gfx::FillAlpha(c, { kStartButton.left + 4, kStartButton.top + 4, kStartButton.right - 4, kStartButton.bottom - 4 }, RGB(6, 32, 74), 110);
        Gfx::DrawPixelText(c, kStartButton, L"시     작", canStart ? White : RGB(150, 160, 180), Gfx::Align::Center, 24, RGB(0xc3, 0x4a, 0x00), 2);
    } else {
        const int mySlot = GameClient::MySlot();
        bool ready = mySlot >= 0 && room.players[mySlot].ready;
        Gfx::DrawPixelText(c, kStartButton, ready ? L"준비 취소" : L"준     비", White, Gfx::Align::Center, 24, RGB(0xc3, 0x4a, 0x00), 2);
    }
    if (mHoverButton == 0) Gfx::DrawRoundFrame(c, { kLeaveButton.left - 2, kLeaveButton.top - 2, kLeaveButton.right + 2, kLeaveButton.bottom + 2 }, 8, HoverFrame, 2);
    if (mHoverButton == 1) Gfx::DrawRoundFrame(c, { kStartButton.left - 2, kStartButton.top - 2, kStartButton.right + 2, kStartButton.bottom + 2 }, 10, HoverFrame, 2);
}
