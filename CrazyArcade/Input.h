#pragma once
#include "Common.h"

enum class KeyState { None, Down, Pressed, Up };

// 매 프레임 256개 가상키 전체를 폴링한다.
// WM_KEYDOWN은 마지막에 누른 키만 반복 전달하지만, 폴링은 키마다 독립적인 상태를 가지므로
// P1(WASD+Space)과 P2(방향키+Enter)가 동시에 눌러도 서로 끊기지 않는다.
class Input {
public:
    static void Initialize(HWND hwnd);
    static void Update();

    // 프레임 사이(폴링 간격보다 짧게)에 눌렸다 떼진 키를 놓치지 않도록 WndProc에서 전달
    static void OnKeyMessage(WPARAM vk);
    // 포커스를 잃으면 눌린 키를 모두 해제
    static void Clear();

    static bool GetKeyDown(int vk) { return sKeys[vk & 0xFF] == KeyState::Down; }
    static bool GetKeyUp(int vk) { return sKeys[vk & 0xFF] == KeyState::Up; }
    // 누르고 있는 동안 true (눌린 첫 프레임 포함)
    static bool GetKey(int vk) { KeyState s = sKeys[vk & 0xFF]; return s == KeyState::Down || s == KeyState::Pressed; }

    static POINT GetMousePosition() { return sMousePos; }
    static bool IsMouseInClient();

private:
    static HWND sHwnd;
    static KeyState sKeys[256];
    static bool sLatched[256];
    static POINT sMousePos;
};
