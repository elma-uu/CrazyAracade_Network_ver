#include "Input.h"

HWND Input::sHwnd = nullptr;
KeyState Input::sKeys[256] = {};
bool Input::sLatched[256] = {};
POINT Input::sMousePos = {};

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
}

bool Input::IsMouseInClient()
{
    RECT rc;
    GetClientRect(sHwnd, &rc);
    return PtInRect(&rc, sMousePos) != FALSE;
}
