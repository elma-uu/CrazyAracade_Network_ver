#include "LobbyScene.h"
#include "SceneManager.h"
#include "NetworkManager.h"
#include "GameClient.h"
#include "Input.h"
#include "Gfx.h"
#include "Game.h"

using namespace Ui;

namespace {
    bool IsEmpty(const GameClient::RoomView& r) { return r.state == ROOM_EMPTY || r.players == 0; }
    bool CanEnter(const GameClient::RoomView& r)
    {
        return IsEmpty(r) || (r.state == ROOM_WAITING && r.players < MAX_ROOM_PLAYER);
    }
}
void LobbyScene::OnEnter()
{
    if (!GameClient::IsLobbyPreview()) {
        GameClient::ClearRoomList();
        GameClient::RequestRoomList();
    }
    mLog.Add(L"[알림] 로비에 들어왔습니다.", Yellow);
    if (GameClient::IsLobbyPreview()) mLog.Add(L"[알림] 미리보기: 서버 없이 샘플 방 목록을 보여 줍니다.", Dim);
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

    UpdateJoin();
    if (GameClient::GetJoinState() == GameClient::JoinState::Joined) return;

    // (F5는 에코 테스트 접속 키라 새로고침에 쓰지 않는다)
    if (Input::GetKeyDown(VK_RETURN)) TryEnter();

    // 화면(1500x900) 좌표 → 캔버스(800x600) 좌표
    POINT p = ToCanvas(Input::GetMousePosition());

    // 마우스가 올라간 방 = 노란 테두리
    mHover = -1;
    if (Input::IsMouseInClient()) {
        for (int i = 0; i < MAX_ROOM; ++i) {
            RECT rc = RoomRect(i);
            if (PtInRect(&rc, p)) { mHover = i; break; }
        }
    }

    if (!Input::GetKeyDown(VK_LBUTTON) || !Input::IsMouseInClient()) return;
    if (mHover >= 0) { SelectRoom(mHover); return; }
    RECT refresh = Box(345, 62, 98, 20);
    RECT quick = Box(647, 429, 122, 45);
    RECT enter = Box(514, 495, 188, 53);
    RECT power = Box(749, 570, 31, 27);   // 오른쪽 아래 전원 버튼
    if (PtInRect(&power, p)) { Game::Quit(); return; }
    if (PtInRect(&refresh, p)) Refresh();
    else if (PtInRect(&quick, p)) QuickEnter();
    else if (PtInRect(&enter, p)) TryEnter();
}

RECT LobbyScene::RoomRect(int index)
{
    const int row = index / 4, col = index % 4;
    const int top = TileTop + RowStep * row;
    // 칸 그림 + 이름 칸 + 상태 칸 전체
    return { SlotL[row][col] - 1, top, SlotR[row][col] + 2, StatusTop + RowStep * row + 16 };
}

void LobbyScene::SelectRoom(int index)
{
    mSelected = index;
    const GameClient::RoomView& r = GameClient::Room(index);
    if (CanEnter(r)) mLog.Add(L"[알림] " + RoomNumber(r.id) + L"번 방을 골랐습니다. 입장을 누르세요.", Yellow);
    else mLog.Add(L"[알림] " + RoomNumber(r.id) + L"번 방은 " + (r.state == ROOM_PLAYING ? L"게임 중" : L"꽉 차서") +
        L" 들어갈 수 없어요.", Pink);
}

void LobbyScene::Refresh()
{
    if (!GameClient::IsLobbyPreview()) GameClient::RequestRoomList();
    mLog.Add(L"[알림] 방 목록을 새로 받았습니다.", Yellow);
}

void LobbyScene::QuickEnter()
{
    // 사람이 남아 있는 대기 방 중 사람이 가장 많은 방으로 바로 들어간다. 없으면 빈 방
    int pick = -1;
    for (int i = 0; i < MAX_ROOM; ++i) {
        const GameClient::RoomView& r = GameClient::Room(i);
        if (IsEmpty(r) || !CanEnter(r)) continue;
        if (pick < 0 || r.players > GameClient::Room(pick).players) pick = i;
    }
    for (int i = 0; i < MAX_ROOM && pick < 0; ++i)
        if (IsEmpty(GameClient::Room(i))) pick = i;

    if (pick < 0) { mLog.Add(L"[알림] 들어갈 수 있는 방이 없어요.", Pink); return; }
    mSelected = pick;
    TryEnter();
}

void LobbyScene::TryEnter()
{
    if (GameClient::GetJoinState() == GameClient::JoinState::Waiting) return;   // 이미 입장 요청 중
    if (mSelected < 0) { mLog.Add(L"[알림] 먼저 방을 골라 주세요.", Pink); return; }
    const GameClient::RoomView& r = GameClient::Room(mSelected);
    if (!CanEnter(r)) { mLog.Add(L"[알림] 이 방에는 들어갈 수 없어요.", Pink); return; }
    mLog.Add(L"[알림] " + RoomNumber(r.id) + L"번 방에 들어가는 중...", Yellow);
    GameClient::RequestJoinRoom(r.id);
}

void LobbyScene::UpdateJoin()
{
    switch (GameClient::GetJoinState()) {
    case GameClient::JoinState::Joined:
        SceneManager::ChangeScene(SceneType::Room);
        break;
    case GameClient::JoinState::Failed:
        mLog.Add(L"[알림] " + GameClient::JoinFailReason(), Pink);
        GameClient::ClearJoinFailure();
        GameClient::RequestRoomList();   // 그사이 방 상태가 바뀌었을 수 있다
        break;
    default:
        break;
    }
}

void LobbyScene::Render(HDC hdc)
{
    HDC c = mCanvas.Begin(hdc);

    Gfx::DrawOpaque(c, BitmapID::LobbyBg, 0, 0, CanvasW, CanvasH, 0, 0, CanvasW, CanvasH);
    DrawRooms(c);
    DrawHeader(c);
    DrawProfile(c);
    DrawSelectedRoom(c);
    DrawButtons(c);
    mLog.Draw(c);
    Gfx::DrawPixelText(c, Box(32, 3, 400, 23), L"[공지] 게임 중이거나 꽉 찬 방(X 칸)은 들어갈 수 없어요",
        Yellow, Gfx::Align::Left);

    mCanvas.Present(hdc);
}

void LobbyScene::DrawRooms(HDC c) const
{
    for (int i = 0; i < MAX_ROOM; ++i) {
        const int row = i / 4, col = i % 4;
        const int L = SlotL[row][col], R = SlotR[row][col];
        const int tileX = L - 1, tileY = TileTop + RowStep * row;
        const int barX = L + 2, barW = R - L - 2;
        const int nameY = NameTop + RowStep * row, statusY = StatusTop + RowStep * row;

        const GameClient::RoomView& r = GameClient::Room(i);
        const bool empty = IsEmpty(r);
        const bool canEnter = CanEnter(r);

        // 들어갈 수 있는 방 = 열린 칸, 게임 중이거나 꽉 찬 방 = X 칸
        Gfx::DrawOpaque(c, BitmapID::LobbyTiles, tileX, tileY, 101, 101, canEnter ? 0 : 101, 0, 101, 101);
        if (empty) {
            DrawSprite(c, RandomIcon, tileX + 26, tileY + 20);
        } else {
            DrawMapIcon(c, r.map, tileX + 19, tileY + 22, MapIconSize::Big);
            if (!canEnter) Gfx::FillAlpha(c, Box(tileX + 19, tileY + 22, 64, 64), RGB(6, 32, 74), 120);
        }
        DrawDigits(c, DigitStyle::Yellow, RoomNumber(r.id), tileX + 7, tileY + 6);

        if (empty) {
            Gfx::DrawPixelText(c, Box(barX, nameY, barW, 18), L"빈 방", Dim);
            Gfx::DrawPixelText(c, Box(barX + 2, statusY, 56, 16), L"입장가능", Dim, Gfx::Align::Left);
        } else {
            DrawSprite(c, CrownSmall, barX + 4, nameY + 1);
            Gfx::DrawPixelText(c, { barX + 25, nameY, barX + barW - 2, nameY + 18 }, r.host, White, Gfx::Align::Left);
            const wchar_t* state = L"대기 중";
            COLORREF stateColor = White;
            if (r.state == ROOM_PLAYING) { state = L"게임 중"; stateColor = Pink; }
            else if (r.players >= MAX_ROOM_PLAYER) { state = L"꽉 참"; stateColor = Pink; }
            Gfx::DrawPixelText(c, Box(barX + 2, statusY, 56, 16), state, stateColor, Gfx::Align::Left);
        }
        // 인원: 상태 칸 오른쪽
        std::wstring count = std::to_wstring(r.players) + L"/" + std::to_wstring(MAX_ROOM_PLAYER);
        DrawDigits(c, DigitStyle::Green, count, barX + barW - 3 - DigitsWidth(DigitStyle::Green, count), statusY + 3);

        // 마우스가 올라간 방은 노란 테두리, 골라 둔 방은 (마우스가 다른 곳에 있을 때) 옅은 하늘색 테두리
        if (i == mHover)
            Gfx::DrawRoundFrame(c, Box(L - 3, tileY - 2, 104, 145), 8, HoverFrame, 2);
        else if (i == mSelected)
            Gfx::DrawRoundFrame(c, Box(L - 3, tileY - 2, 104, 145), 8, SelectFrame, 2);
    }
}

void LobbyScene::DrawHeader(HDC c) const
{
    // 왼쪽 번호 칸에는 고른 방 번호
    if (mSelected >= 0) DrawDigits(c, DigitStyle::Yellow, RoomNumber(GameClient::Room(mSelected).id), 31, 66);
    Gfx::DrawPixelText(c, Box(80, 59, 60, 26), L"방 목록", White, Gfx::Align::Left);
    Gfx::DrawPixelText(c, Box(144, 59, 196, 26), L"1~8번 방 · 눌러서 고르기", Dim, Gfx::Align::Left);
    Gfx::DrawPixelText(c, Box(345, 62, 98, 20), L"새로고침", White);
}

void LobbyScene::DrawProfile(HDC c) const
{
    Gfx::DrawPixelText(c, Box(496, 186, 120, 22), L"내 정보", Label, Gfx::Align::Left);

    const int wins = GameClient::Wins(), losses = GameClient::Losses();
    std::wstring record = std::to_wstring(wins) + L"승 " + std::to_wstring(losses) + L"패";
    std::wstring rate = (wins + losses > 0) ? std::to_wstring(wins * 100 / (wins + losses)) + L"%" : L"-";
    DrawKeyValue(c, L"닉네임", GameClient::MyName(), 504, 212, 554, 566, 760);
    DrawKeyValue(c, L"전적", record, 504, 236, 554, 566, 760);
    DrawKeyValue(c, L"승률", rate, 504, 260, 554, 566, 760);

    Gfx::DrawPixelText(c, Box(486, 292, 290, 24), L"로비에서는 캐릭터·색을 고르지 않아요", Dim);
}

void LobbyScene::DrawSelectedRoom(HDC c) const
{
    if (mSelected < 0) {
        Gfx::DrawPixelText(c, Box(646, 347, 126, 16), L"방을 골라 주세요", Label);
        DrawKeyValue(c, L"인원", L"-", 654, 366, 686, 696, 770);
        DrawKeyValue(c, L"방장", L"-", 654, 385, 686, 696, 770);
        DrawKeyValue(c, L"상태", L"-", 654, 404, 686, 696, 770);
        return;
    }

    const GameClient::RoomView& r = GameClient::Room(mSelected);
    const bool empty = IsEmpty(r);
    Gfx::DrawPixelText(c, Box(646, 347, 126, 16), RoomNumber(r.id) + L"번 방", Label);
    DrawKeyValue(c, L"인원", std::to_wstring(r.players) + L" / " + std::to_wstring(MAX_ROOM_PLAYER), 654, 366, 686, 696, 770);

    if (empty) {
        DrawSprite(c, RandomIcon, 538, 367);
        Gfx::DrawPixelText(c, Box(520, 429, 86, 29), L"빈 방", Yellow);
        DrawKeyValue(c, L"방장", L"-", 654, 385, 686, 696, 770);
        DrawKeyValue(c, L"상태", L"입장 가능", 654, 404, 686, 696, 770);
        return;
    }

    DrawMapIcon(c, r.map, 534, 368, MapIconSize::Mid);
    Gfx::DrawPixelText(c, Box(520, 429, 86, 29), MapName(r.map), Yellow);
    DrawKeyValue(c, L"방장", L"", 654, 385, 686, 696, 770);
    DrawSprite(c, CrownTiny, 696, 387);
    Gfx::DrawPixelText(c, { 714, 385, 768, 401 }, r.host, White, Gfx::Align::Left);
    const wchar_t* state = (r.state == ROOM_PLAYING) ? L"게임 중"
        : (r.players >= MAX_ROOM_PLAYER) ? L"꽉 참" : L"대기 중";
    DrawKeyValue(c, L"상태", state, 654, 404, 686, 696, 770);
}

void LobbyScene::DrawButtons(HDC c) const
{
    // 버튼 글씨는 원래 버튼 글씨 크기·테두리 색에 맞춘다
    Gfx::DrawPixelText(c, Box(647, 429, 122, 45), L"빠른 입장", White, Gfx::Align::Center, 18, RGB(0x0b, 0x3f, 0x86), 2);
    Gfx::DrawPixelText(c, Box(514, 495, 188, 53), L"입     장", White, Gfx::Align::Center, 24, RGB(0xc3, 0x4a, 0x00), 2);
}
