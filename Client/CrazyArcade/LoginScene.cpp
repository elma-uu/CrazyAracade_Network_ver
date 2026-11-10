#include "LoginScene.h"
#include "SceneManager.h"
#include "Input.h"
#include "Gfx.h"

void LoginScene::OnEnter()
{
}

void LoginScene::Update(float)
{
    if (Input::GetKeyDown(VK_ESCAPE)) {
        SceneManager::ChangeScene(SceneType::Title);
        return;
    }
    // 온라인 인게임이 완성될 때까지 기존 2인 모드도 들어갈 수 있게 남겨 둔다
    if (Input::GetKeyDown(VK_F9)) {
        SceneManager::ChangeScene(SceneType::Select);
        return;
    }
}

void LoginScene::Render(HDC hdc)
{
    Gfx::DrawOpaque(hdc, BitmapID::Login, 0, 0, Config::ScreenWidth, Config::ScreenHeight, 0, 0, 800, 600);
    Gfx::DrawDarkOverlay(hdc, Config::ScreenWidth, Config::ScreenHeight);

    Gfx::DrawOutlinedText(hdc, 640, 250, L"로그인");
    Gfx::DrawSmallText(hdc, 20, Config::ScreenHeight - 40, L"ESC 타이틀로    F9 한 컴퓨터 2인 모드");
}
