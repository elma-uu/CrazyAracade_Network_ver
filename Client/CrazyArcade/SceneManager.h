#pragma once
#include "Scene.h"

enum class SceneType { Login, Select, Forest, City, Ice, Count };

class SceneManager {
public:
    template <typename T>
    static void Register(SceneType type)
    {
        sFactories[static_cast<int>(type)] = [] { return std::unique_ptr<Scene>(new T()); };
    }

    // 전환은 현재 프레임이 끝난 뒤 적용된다 (Update 도중 자기 자신이 파괴되는 일 방지).
    // 씬은 진입할 때마다 새로 생성되므로 이전 판의 상태가 남지 않는다.
    static void ChangeScene(SceneType type);

    static void Update(float deltaTime);
    static void FixedUpdate();
    static void Render(HDC hdc);
    static void Release();

    // 이번 프레임에 씬이 바뀌었으면 true (고정 스텝 누적 시간 초기화용)
    static bool ApplyPendingChange();

private:
    static std::function<std::unique_ptr<Scene>()> sFactories[static_cast<int>(SceneType::Count)];
    static std::unique_ptr<Scene> sActive;
    static SceneType sPending;
    static bool sHasPending;
};
