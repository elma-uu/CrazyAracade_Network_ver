#include "SceneManager.h"
#include "StageScene.h"
#include <optional>

namespace {
    std::optional<S2C_MapInfo> pendingMap;
}

std::function<std::unique_ptr<Scene>()> SceneManager::sFactories[static_cast<int>(SceneType::Count)];
std::unique_ptr<Scene> SceneManager::sActive;
SceneType SceneManager::sPending = SceneType::Login;
bool SceneManager::sHasPending = false;

void SceneManager::ChangeScene(SceneType type)
{
    pendingMap.reset();
    sPending = type;
    sHasPending = true;
}

/* 수정 - 2026-11-11 */
// 수신부가 완성된 S2C_MapInfo를 전달한다. 씬 전환 예약 중이면 OnEnter 뒤에 적용한다.
bool SceneManager::OnMapInfo(const S2C_MapInfo& packet)
{
    if (packet.size != sizeof(packet) || packet.type != S2C_MAP_INFO)
        return false;

    SceneType target;
    switch (packet.mapId) {
    case MAP_FOREST: target = SceneType::Forest; break;
    case MAP_CITY:   target = SceneType::City;   break;
    case MAP_ICE:    target = SceneType::Ice;    break;
    default: return false;
    }
    if (target != sPending) return false;
    if (sHasPending) {
        pendingMap = packet;
        return true;
    }
    auto* stage = dynamic_cast<StageScene*>(sActive.get());
    if (!stage) return false;
    stage->ApplyMapInfo(packet);
    return true;
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
        if (pendingMap) OnMapInfo(*pendingMap);
    }
    pendingMap.reset();
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
    pendingMap.reset();
    if (sActive) sActive->OnExit();
    sActive.reset();
    sHasPending = false;
}
