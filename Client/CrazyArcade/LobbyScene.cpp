#include "LobbyScene.h"
#include "SceneManager.h"
#include "NetworkManager.h"
#include "GameClient.h"
#include "Input.h"
#include "Gfx.h"

void LobbyScene::Update(float)
{
    if (Input::GetKeyDown(VK_ESCAPE)) {
        NetworkManager::Disconnect();
        GameClient::ResetLogin();
        SceneManager::ChangeScene(SceneType::Title);
        return;
    }
    // 서버와 끊기면 로그인 화면으로 (끊김 안내는 GameClient가 띄운다)
    if (!NetworkManager::IsConnected()) {
        SceneManager::ChangeScene(SceneType::Login);
        return;
    }
}

void LobbyScene::Render(HDC hdc)
{
    Gfx::DrawOpaque(hdc, BitmapID::Login, 0, 0, Config::ScreenWidth, Config::ScreenHeight, 0, 0, 800, 600);
    Gfx::DrawDarkOverlay(hdc, Config::ScreenWidth, Config::ScreenHeight);

    Gfx::DrawOutlinedText(hdc, 700, 260, L"로비");
    Gfx::DrawSmallText(hdc, 600, 380, GameClient::MyName() + L"   " +
        std::to_wstring(GameClient::Wins()) + L"승 " + std::to_wstring(GameClient::Losses()) + L"패");
    Gfx::DrawSmallText(hdc, 600, 430, L"방 목록 준비 중", RGB(200, 200, 200));
    Gfx::DrawSmallText(hdc, 20, Config::ScreenHeight - 40, L"ESC 로그아웃");
}
