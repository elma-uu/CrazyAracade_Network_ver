#include "SceneManager.h"

std::function<std::unique_ptr<Scene>()> SceneManager::sFactories[static_cast<int>(SceneType::Count)];
std::unique_ptr<Scene> SceneManager::sActive;
SceneType SceneManager::sPending = SceneType::Title;
bool SceneManager::sHasPending = false;

void SceneManager::ChangeScene(SceneType type)
{
    sPending = type;
    sHasPending = true;
}

bool SceneManager::ApplyPendingChange()
{
    if (!sHasPending) return false;
    sHasPending = false;

    if (sActive) sActive->OnExit();
    sActive.reset();

    auto& factory = sFactories[static_cast<int>(sPending)];
    if (factory) {
        sActive = factory();
        sActive->OnEnter();
    }
    return true;
}

void SceneManager::Update(float deltaTime)
{
    if (sActive) sActive->Update(deltaTime);
}

void SceneManager::FixedUpdate()
{
    if (sActive) sActive->FixedUpdate();
}

void SceneManager::Render(HDC hdc)
{
    if (sActive) sActive->Render(hdc);
}

void SceneManager::Release()
{
    if (sActive) sActive->OnExit();
    sActive.reset();
    sHasPending = false;
}
