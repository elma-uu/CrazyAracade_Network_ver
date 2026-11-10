#include "TitleScene.h"
#include "SceneManager.h"
#include "AudioManager.h"
#include "Input.h"
#include "Game.h"
#include "Gfx.h"

void TitleScene::OnEnter()
{
    Audio::PlayMusic("Login.wav");
}

void TitleScene::Update(float)
{
    if (!Input::GetKeyDown(VK_LBUTTON) || !Input::IsMouseInClient()) return;

    if (Input::GetMousePosition().x < 800) {
        Audio::PlayMusic("Menu.wav");
        SceneManager::ChangeScene(SceneType::Login);
    } else {
        Game::Quit();
    }
}

void TitleScene::Render(HDC hdc)
{
    Gfx::DrawOpaque(hdc, BitmapID::Login, 0, 0, Config::ScreenWidth, Config::ScreenHeight, 0, 0, 800, 600);
}
