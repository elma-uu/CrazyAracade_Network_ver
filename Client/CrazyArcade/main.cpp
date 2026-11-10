#include "Game.h"
#include "SceneManager.h"
#include "TitleScene.h"
#include "SelectScene.h"
#include "ForestStage.h"
#include "CityStage.h"
#include "IceStage.h"

static void RegisterScenes()
{
    SceneManager::Register<TitleScene>(SceneType::Title);
    SceneManager::Register<SelectScene>(SceneType::Select);
    SceneManager::Register<ForestStage>(SceneType::Forest);
    SceneManager::Register<CityStage>(SceneType::City);
    SceneManager::Register<IceStage>(SceneType::Ice);
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow)
{
    if (!Game::Initialize(hInstance, nCmdShow)) return 0;

    RegisterScenes();
    SceneManager::ChangeScene(SceneType::Title);

    return Game::Run();
}
