#include "GameClient.h"
#include "NetworkManager.h"
#include "Input.h"
#include "Gfx.h"
#include <cstdio>

std::wstring GameClient::sNotice;
ULONGLONG GameClient::sNoticeUntil = 0;
bool GameClient::sEchoTestVisible = false;
int GameClient::sEchoSent = 0;
int GameClient::sEchoReceived = 0;
std::wstring GameClient::sEchoStatus;
std::wstring GameClient::sLastEcho;

namespace {
    constexpr ULONGLONG kNoticeMs = 3000;
}

void GameClient::Update()
{
    UpdateEchoTest();

    // 서버 패킷을 먼저 반영한 뒤 씬을 갱신한다
    NetworkManager::Poll();
    if (NetworkManager::ConsumeConnectionLost())
        ShowNotice(L"연결 끊김: " + NetworkManager::LastError());
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
