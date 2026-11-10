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

    // ---- 글자 입력 (닉네임 등) ----
    // WndProc의 WM_CHAR에서 전달. 키 반복·한글 조합 결과까지 그대로 들어온다
    static void OnCharMessage(wchar_t ch);
    // 이번 프레임에 입력된 글자들 ('\b' 백스페이스, '\r' 엔터, '\t' 탭 포함)
    static const std::wstring& GetTypedText() { return sTypedText; }
    // 입력칸이 있는 화면에서만 켠다: IME(한글 입력)를 켜고 끔.
    // 게임 중에는 한글 IME가 WASD를 가로채지 않도록 꺼 둔다
    static void SetTextInputEnabled(bool enabled);

private:
    static HWND sHwnd;
    static KeyState sKeys[256];
    static bool sLatched[256];
    static POINT sMousePos;
    static std::wstring sPendingText;   // 프레임 사이에 WM_CHAR로 쌓인 글자
    static std::wstring sTypedText;     // 이번 프레임에 보여 줄 글자
};
