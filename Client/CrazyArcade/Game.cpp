#include "Game.h"
#include "Input.h"
#include "Time.h"
#include "SceneManager.h"
#include "ResourceManager.h"
#include "AudioManager.h"
#include "Gfx.h"
#include "NetworkManager.h"
#include <ctime>
#include <imm.h>
#include <timeapi.h>
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "winmm.lib")

HWND Game::sHwnd = nullptr;
HDC Game::sBackDC = nullptr;
HBITMAP Game::sBackBitmap = nullptr;
HGDIOBJ Game::sOldBackBitmap = nullptr;
float Game::sFixedAccumulatorMs = 0.0f;

namespace {
    const wchar_t* kWindowClass = L"CrazyArcadeWindow";
    const wchar_t* kWindowTitle = L"CrazyArcade";
    constexpr DWORD kWindowStyle = WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    constexpr DWORD kFrameMs = 8;  // 약 120fps로 제한 (로직은 고정 스텝이라 프레임과 무관)

    // 접속 실패·연결 끊김 안내: 몇 초 동안 화면 가운데에 띄운다
    namespace Notice {
        constexpr ULONGLONG kShowMs = 3000;
        std::wstring sText;
        ULONGLONG sUntil = 0;

        void Show(const std::wstring& text)
        {
            sText = text;
            sUntil = GetTickCount64() + kShowMs;
        }

        void Render(HDC hdc)
        {
            if (GetTickCount64() < sUntil) Gfx::DrawNoticeBar(hdc, sText);
        }
    }

    // 1단계 에코 테스트용: F5 = 127.0.0.1 접속, F6 = 에코 보내기, 결과를 화면 왼쪽 위에 표시
    namespace EchoTest {
        bool sVisible = false;
        int sSent = 0;
        std::wstring sStatus;

        void Update()
        {
            if (Input::GetKeyDown(VK_F5)) {
                sVisible = true;
                if (NetworkManager::Connect("127.0.0.1", SERVER_PORT)) {
                    sStatus = L"접속 성공";
                } else {
                    sStatus = L"접속 실패";
                    Notice::Show(L"접속 실패: " + NetworkManager::LastError());
                }
            }
            if (Input::GetKeyDown(VK_F6) && NetworkManager::IsConnected()) {
                char msg[64];
                sprintf_s(msg, "echo test #%d", ++sSent);
                NetworkManager::SendEcho(msg);
            }
        }

        void Render(HDC hdc)
        {
            if (!sVisible) return;
            const std::string& echo = NetworkManager::LastEcho();
            std::wstring last(echo.begin(), echo.end());   // 에코 테스트 문자열은 ASCII
            Gfx::DrawSmallText(hdc, 10, 10, L"[F5] 접속  [F6] 에코  |  " + sStatus +
                (NetworkManager::IsConnected() ? L" (연결됨)" : L" (연결 안 됨)"));
            Gfx::DrawSmallText(hdc, 10, 36, L"보냄 " + std::to_wstring(sSent) +
                L" / 받음 " + std::to_wstring(NetworkManager::EchoCount()) + L"  마지막: " + last);
        }
    }
}

bool Game::Initialize(HINSTANCE hInstance, int nCmdShow)
{
    srand(static_cast<unsigned>(time(nullptr)));
    ResourceManager::LoadAll(hInstance);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    wc.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&wc)) return false;

    // 클라이언트 영역이 정확히 백버퍼 크기가 되도록 창 크기 계산
    RECT rect = { 0, 0, Config::ScreenWidth, Config::ScreenHeight };
    AdjustWindowRect(&rect, kWindowStyle, FALSE);
    sHwnd = CreateWindowW(kWindowClass, kWindowTitle, kWindowStyle, 0, 0,
        rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, hInstance, nullptr);
    if (!sHwnd) return false;

    // 한글 IME가 켜져 있어도 WASD가 게임 키로 들어오도록 이 창에서는 IME를 끈다
    ImmAssociateContext(sHwnd, nullptr);

    HDC screen = GetDC(sHwnd);
    sBackDC = CreateCompatibleDC(screen);
    sBackBitmap = CreateCompatibleBitmap(screen, Config::ScreenWidth, Config::ScreenHeight);
    sOldBackBitmap = SelectObject(sBackDC, sBackBitmap);
    ReleaseDC(sHwnd, screen);

    Input::Initialize(sHwnd);
    Time::Initialize();
    if (!Audio::Initialize())
        MessageBoxW(sHwnd, L"FMOD audio initialization failed.", L"Audio", MB_OK | MB_ICONWARNING);

    ShowWindow(sHwnd, nCmdShow);
    UpdateWindow(sHwnd);
    return true;
}

int Game::Run()
{
    timeBeginPeriod(1);
    MSG msg = {};
    bool running = true;

    while (running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!running || !sHwnd) break;

        DWORD frameStart = timeGetTime();
        if (IsIconic(sHwnd)) {
            // 최소화 중에는 게임을 멈춘다
            Time::Update();
            Sleep(10);
            continue;
        }
        Frame();

        DWORD elapsed = timeGetTime() - frameStart;
        if (elapsed < kFrameMs) Sleep(kFrameMs - elapsed);
    }

    timeEndPeriod(1);
    Shutdown();
    return static_cast<int>(msg.wParam);
}

void Game::Frame()
{
    // 씬 전환은 프레임 경계에서만. 새 씬 로딩에 걸린 시간은 고정 스텝으로 따라잡지 않는다
    bool sceneChanged = SceneManager::ApplyPendingChange();

    Input::Update();
    Time::Update();
    Audio::Update();
    // 서버 패킷을 먼저 반영한 뒤 씬을 갱신한다
    EchoTest::Update();
    NetworkManager::Poll();
    if (NetworkManager::ConsumeConnectionLost())
        Notice::Show(L"연결 끊김: " + NetworkManager::LastError());

    float deltaMs = sceneChanged ? 0.0f : Time::DeltaMs();
    if (sceneChanged) sFixedAccumulatorMs = 0.0f;

    SceneManager::Update(deltaMs / 1000.0f);

    sFixedAccumulatorMs += deltaMs;
    while (sFixedAccumulatorMs >= Config::FixedStepMs) {
        SceneManager::FixedUpdate();
        sFixedAccumulatorMs -= Config::FixedStepMs;
    }

    RECT rc = { 0, 0, Config::ScreenWidth, Config::ScreenHeight };
    FillRect(sBackDC, &rc, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    SceneManager::Render(sBackDC);
    EchoTest::Render(sBackDC);
    Notice::Render(sBackDC);

    HDC hdc = GetDC(sHwnd);
    Present(hdc);
    ReleaseDC(sHwnd, hdc);
}

void Game::Present(HDC hdc)
{
    BitBlt(hdc, 0, 0, Config::ScreenWidth, Config::ScreenHeight, sBackDC, 0, 0, SRCCOPY);
}

void Game::Quit()
{
    // 프레임 도중 창이 파괴되지 않도록 메시지로 넘긴다
    if (sHwnd) PostMessage(sHwnd, WM_CLOSE, 0, 0);
}

void Game::Shutdown()
{
    SceneManager::Release();
    NetworkManager::Shutdown();
    Audio::Shutdown();
    Gfx::Release();
    ResourceManager::UnloadAll();

    if (sBackDC) {
        SelectObject(sBackDC, sOldBackBitmap);
        DeleteDC(sBackDC);
    }
    if (sBackBitmap) DeleteObject(sBackBitmap);
    sBackDC = nullptr;
    sBackBitmap = nullptr;
}

LRESULT CALLBACK Game::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        Input::OnKeyMessage(wParam);
        break;
    case WM_KILLFOCUS:
        Input::Clear();
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        if (sBackDC) Present(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        sHwnd = nullptr;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
