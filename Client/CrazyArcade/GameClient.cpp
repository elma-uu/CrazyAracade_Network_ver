#include "GameClient.h"
#include "NetworkManager.h"
#include "Input.h"
#include "Gfx.h"
#include <cstdio>

std::wstring GameClient::sMyName;
std::wstring GameClient::sServerIp = L"127.0.0.1";
GameClient::LoginState GameClient::sLoginState = GameClient::LoginState::None;
ULONGLONG GameClient::sLoginRequestedAt = 0;
int GameClient::sMyId = -1;
int GameClient::sWins = 0;
int GameClient::sLosses = 0;
GameClient::RoomView GameClient::sRooms[MAX_ROOM];
bool GameClient::sHasRoomList = false;
std::wstring GameClient::sNotice;
ULONGLONG GameClient::sNoticeUntil = 0;
bool GameClient::sEchoTestVisible = false;
int GameClient::sEchoSent = 0;
int GameClient::sEchoReceived = 0;
std::wstring GameClient::sEchoStatus;
std::wstring GameClient::sLastEcho;

namespace {
    constexpr ULONGLONG kNoticeMs = 3000;

    // 서버가 보낸 UTF-8 이름(끝 '\0' 없을 수도 있음) → 화면용 wstring
    std::wstring Utf8NameToWide(const char* name, size_t maxLen)
    {
        int bytes = static_cast<int>(strnlen(name, maxLen));
        if (bytes == 0) return {};
        int chars = MultiByteToWideChar(CP_UTF8, 0, name, bytes, nullptr, 0);
        std::wstring wide(chars, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, name, bytes, wide.data(), chars);
        return wide;
    }
}

void GameClient::Update()
{
    UpdateEchoTest();

    // 서버 패킷을 먼저 반영한 뒤 씬을 갱신한다
    NetworkManager::Poll();
    if (NetworkManager::ConsumeConnectionLost()) {
        ResetLogin();
        ShowNotice(L"연결 끊김: " + NetworkManager::LastError());
    }
}

void GameClient::RenderOverlay(HDC hdc)
{
    RenderEchoTest(hdc);
    if (GetTickCount64() < sNoticeUntil) Gfx::DrawNoticeBar(hdc, sNotice);
}

void GameClient::ShowNotice(const std::wstring& text)
{
    sNotice = text;
    sNoticeUntil = GetTickCount64() + kNoticeMs;
}

void GameClient::SetLoginInfo(const std::wstring& name, const std::wstring& serverIp)
{
    sMyName = name;
    sServerIp = serverIp;
}

std::string GameClient::MyNameUtf8()
{
    if (sMyName.empty()) return {};
    int bytes = WideCharToMultiByte(CP_UTF8, 0, sMyName.c_str(), static_cast<int>(sMyName.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(bytes, '\0');
    WideCharToMultiByte(CP_UTF8, 0, sMyName.c_str(), static_cast<int>(sMyName.size()), utf8.data(), bytes, nullptr, nullptr);
    return utf8;
}

void GameClient::RequestLogin()
{
    NetworkManager::SendLogin(MyNameUtf8());
    sLoginState = LoginState::Waiting;
    sLoginRequestedAt = GetTickCount64();
}

void GameClient::ResetLogin()
{
    sLoginState = LoginState::None;
    sMyId = -1;
    sWins = sLosses = 0;   // 다음 로그인에 이전 사람의 전적이 보이지 않게
}

void GameClient::OnLoginResult(const S2C_LoginResult* p)
{
    // 요청하지 않은 응답은 무시 (늦게 온 응답 등)
    if (sLoginState != LoginState::Waiting) return;

    if (!p->success) {
        sLoginState = LoginState::Failed;
        return;
    }
    sLoginState = LoginState::LoggedIn;
    sMyId = p->id;
    sWins = p->win;
    sLosses = p->lose;
}

void GameClient::RequestRoomList()
{
    NetworkManager::SendRoomList();
}

void GameClient::ClearRoomList()
{
    for (int i = 0; i < MAX_ROOM; ++i) {
        sRooms[i] = RoomView{};
        sRooms[i].id = i + 1;
    }
    sHasRoomList = false;
}

void GameClient::OnRoomList(const S2C_RoomList* p)
{
    for (int i = 0; i < MAX_ROOM; ++i) {
        const ROOM_SUMMARY& s = p->rooms[i];
        RoomView& r = sRooms[i];
        r.id = s.roomId;
        r.map = s.mapId;
        r.players = std::min<int>(s.playerCount, MAX_ROOM_PLAYER);
        r.state = s.state;
        r.host = Utf8NameToWide(s.hostName, sizeof(s.hostName));
    }
    sHasRoomList = true;
}

void GameClient::OnEcho(const S2C_Echo* p)
{
    std::string msg(p->message, strnlen(p->message, sizeof(p->message)));
    sLastEcho.assign(msg.begin(), msg.end());   // 에코 테스트 문자열은 ASCII
    ++sEchoReceived;
}

void GameClient::UpdateEchoTest()
{
    if (Input::GetKeyDown(VK_F5)) {
        sEchoTestVisible = true;
        sEchoSent = sEchoReceived = 0;
        sLastEcho.clear();
        if (NetworkManager::Connect("127.0.0.1", SERVER_PORT)) {
            sEchoStatus = L"접속 성공";
        } else {
            sEchoStatus = L"접속 실패";
            ShowNotice(L"접속 실패: " + NetworkManager::LastError());
        }
    }
    if (Input::GetKeyDown(VK_F6) && NetworkManager::IsConnected()) {
        char msg[64];
        sprintf_s(msg, "echo test #%d", ++sEchoSent);
        NetworkManager::SendEcho(msg);
    }
}

void GameClient::RenderEchoTest(HDC hdc)
{
    if (!sEchoTestVisible) return;
    Gfx::DrawSmallText(hdc, 10, 10, L"[F5] 접속  [F6] 에코  |  " + sEchoStatus +
        (NetworkManager::IsConnected() ? L" (연결됨)" : L" (연결 안 됨)"));
    Gfx::DrawSmallText(hdc, 10, 36, L"보냄 " + std::to_wstring(sEchoSent) +
        L" / 받음 " + std::to_wstring(sEchoReceived) + L"  마지막: " + sLastEcho);
}
