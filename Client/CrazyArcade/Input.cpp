#include "Input.h"
#include <imm.h>
#pragma comment(lib, "imm32.lib")

HWND Input::sHwnd = nullptr;
KeyState Input::sKeys[256] = {};
bool Input::sLatched[256] = {};
POINT Input::sMousePos = {};
std::wstring Input::sPendingText;
std::wstring Input::sTypedText;

void Input::Initialize(HWND hwnd)
{
    sHwnd = hwnd;
    Clear();
}

void Input::OnKeyMessage(WPARAM vk)
{
    if (vk < 256) sLatched[vk] = true;
}

void Input::Clear()
{
    for (int vk = 0; vk < 256; ++vk) {
        sKeys[vk] = KeyState::None;
        sLatched[vk] = false;
    }
    sPendingText.clear();
    sTypedText.clear();
}

void Input::OnCharMessage(wchar_t ch)
{
    sPendingText.push_back(ch);
}

void Input::SetTextInputEnabled(bool enabled)
{
    if (!sHwnd) return;
    if (enabled) ImmAssociateContextEx(sHwnd, nullptr, IACE_DEFAULT);   // 창 기본 IME 다시 연결
    else         ImmAssociateContext(sHwnd, nullptr);                  // IME 끄기
    sPendingText.clear();
}

void Input::Update()
{
    const bool focused = GetForegroundWindow() == sHwnd;

    for (int vk = 1; vk < 256; ++vk) {
        bool down = focused && ((GetAsyncKeyState(vk) & 0x8000) || sLatched[vk]);
        sLatched[vk] = false;

        KeyState& state = sKeys[vk];
        bool wasDown = state == KeyState::Down || state == KeyState::Pressed;
        if (down) state = wasDown ? KeyState::Pressed : KeyState::Down;
        else      state = wasDown ? KeyState::Up : KeyState::None;
    }

    POINT pos;
    if (GetCursorPos(&pos) && ScreenToClient(sHwnd, &pos)) sMousePos = pos;

    sTypedText.swap(sPendingText);
    sPendingText.clear();
}

bool Input::IsMouseInClient()
{
    RECT rc;
    GetClientRect(sHwnd, &rc);
    return PtInRect(&rc, sMousePos) != FALSE;
}
