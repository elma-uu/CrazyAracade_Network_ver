#include "LoginScene.h"
#include "SceneManager.h"
#include "Input.h"
#include "Gfx.h"
#include "NetworkManager.h"
#include "GameClient.h"

namespace {
    // 입력칸 위치 (1500x900 화면 가운데)
    constexpr int kBoxX = 560;
    constexpr int kBoxW = 420;
    constexpr int kBoxH = 46;
    const int kBoxY[2] = { 380, 460 };
    const wchar_t* kLabels[2] = { L"닉네임", L"서버 IP" };

    constexpr int kMaxNameBytes = MAX_NAME_LEN - 1;   // UTF-8 기준, '\0' 자리 제외
    constexpr size_t kMaxIpChars = 15;                // "255.255.255.255"

    RECT BoxRect(int field)
    {
        return { kBoxX, kBoxY[field], kBoxX + kBoxW, kBoxY[field] + kBoxH };
    }

    int Utf8Bytes(const std::wstring& text)
    {
        if (text.empty()) return 0;
        return WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    }
}

void LoginScene::OnEnter()
{
    Input::SetTextInputEnabled(true);
    // 전에 입력한 값이 있으면 다시 채워 준다
    mName = GameClient::MyName();
    mIp = GameClient::ServerIp();
}

void LoginScene::OnExit()
{
    Input::SetTextInputEnabled(false);
}

void LoginScene::Update(float)
{
    // 로그인 전에 화면을 떠나면 접속도 끊는다 (서버에 닉네임 없는 세션이 남지 않게)
    if (Input::GetKeyDown(VK_ESCAPE)) {
        NetworkManager::Disconnect();
        SceneManager::ChangeScene(SceneType::Title);
        return;
    }
    // 온라인 인게임이 완성될 때까지 기존 2인 모드도 들어갈 수 있게 남겨 둔다
    if (Input::GetKeyDown(VK_F9)) {
        NetworkManager::Disconnect();
        SceneManager::ChangeScene(SceneType::Select);
        return;
    }

    if (mConnectPending) {
        ConnectToServer();
        return;
    }
    if (mConnected && !NetworkManager::IsConnected()) {   // 로그인 응답 전에 끊김
        mConnected = false;
        mMessage = L"연결 끊김: " + NetworkManager::LastError();
        mMessageColor = RGB(255, 120, 120);
    }
    // 접속된 뒤에는 로그인 응답을 기다리므로 입력을 막는다
    if (mConnected) return;

    HandleClick();
    HandleTyping();
}

void LoginScene::HandleClick()
{
    if (!Input::GetKeyDown(VK_LBUTTON)) return;
    POINT mouse = Input::GetMousePosition();
    for (int field = 0; field < FieldCount; ++field) {
        RECT rc = BoxRect(field);
        if (PtInRect(&rc, mouse)) mFocus = field;
    }
}

void LoginScene::HandleTyping()
{
    for (wchar_t ch : Input::GetTypedText()) {
        switch (ch) {
        case L'\r': Submit(); return;
        case L'\t': mFocus = (mFocus + 1) % FieldCount; break;
        case L'\b': {
            std::wstring& text = (mFocus == FieldName) ? mName : mIp;
            if (!text.empty()) text.pop_back();
            break;
        }
        default:
            TypeChar(ch);
            break;
        }
    }
}

void LoginScene::TypeChar(wchar_t ch)
{
    if (ch < 0x20 || ch == 0x7F) return;   // 나머지 제어 문자

    if (mFocus == FieldIp) {
        if (((ch >= L'0' && ch <= L'9') || ch == L'.') && mIp.size() < kMaxIpChars)
            mIp.push_back(ch);
        return;
    }

    // 전적 파일이 "닉네임 승 패"를 공백으로 나누므로 공백은 받지 않는다
    if (ch == L' ') {
        mMessage = L"닉네임에는 공백을 쓸 수 없습니다";
        return;
    }
    // 이모지 등 서로게이트 쌍은 백스페이스로 반쪽만 지워질 수 있어 받지 않는다
    if (ch >= 0xD800 && ch <= 0xDFFF) return;

    if (Utf8Bytes(mName + ch) > kMaxNameBytes) {
        mMessage = L"닉네임은 한글 5자, 영문·숫자 15자까지입니다";
        return;
    }
    mName.push_back(ch);
    mMessage.clear();
    mMessageColor = RGB(255, 120, 120);
}

void LoginScene::Submit()
{
    if (mName.empty()) {
        mFocus = FieldName;
        mMessage = L"닉네임을 입력하세요";
        return;
    }
    if (mIp.empty()) {
        mFocus = FieldIp;
        mMessage = L"서버 IP를 입력하세요";
        return;
    }
    mMessage = L"서버에 접속하는 중...";
    mMessageColor = RGB(255, 255, 255);
    mConnectPending = true;
}

void LoginScene::ConnectToServer()
{
    mConnectPending = false;
    mMessageColor = RGB(255, 120, 120);

    std::string ip;   // 숫자와 점만 입력받으므로 ASCII 그대로 옮긴다
    for (wchar_t ch : mIp) ip.push_back(static_cast<char>(ch));
    if (!NetworkManager::Connect(ip.c_str(), SERVER_PORT)) {
        mMessage = L"접속 실패: " + NetworkManager::LastError();
        return;
    }

    GameClient::SetLoginInfo(mName, mIp);
    mConnected = true;
    mMessage = L"접속 성공! 로그인 응답을 기다리는 중...";
    mMessageColor = RGB(140, 230, 140);
    // TODO(11/10): NetworkManager::SendLogin → S2C_LoginResult 받으면 로비로
}

void LoginScene::Render(HDC hdc)
{
    Gfx::DrawOpaque(hdc, BitmapID::Login, 0, 0, Config::ScreenWidth, Config::ScreenHeight, 0, 0, 800, 600);
    Gfx::DrawDarkOverlay(hdc, Config::ScreenWidth, Config::ScreenHeight);

    Gfx::DrawOutlinedText(hdc, 680, 260, L"로그인");

    const bool caretOn = (GetTickCount64() / 500) % 2 == 0;
    for (int field = 0; field < FieldCount; ++field) {
        RECT rc = BoxRect(field);
        Gfx::DrawSmallText(hdc, kBoxX - 110, rc.top + 11, kLabels[field]);

        FillRect(hdc, &rc, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        if (field == mFocus) {
            // 선택된 칸은 주황 테두리 두 줄
            HBRUSH border = CreateSolidBrush(RGB(255, 165, 0));
            FrameRect(hdc, &rc, border);
            RECT inner = { rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1 };
            FrameRect(hdc, &inner, border);
            DeleteObject(border);
        }

        std::wstring text = (field == FieldName) ? mName : mIp;
        if (field == mFocus && caretOn) text += L"|";
        Gfx::DrawSmallText(hdc, rc.left + 12, rc.top + 11, text, RGB(30, 30, 30));
    }

    if (!mMessage.empty())
        Gfx::DrawSmallText(hdc, kBoxX, kBoxY[FieldIp] + kBoxH + 24, mMessage, mMessageColor);

    Gfx::DrawSmallText(hdc, kBoxX, kBoxY[FieldIp] + kBoxH + 70, L"Tab 칸 이동    Enter 접속", RGB(200, 200, 200));
    Gfx::DrawSmallText(hdc, 20, Config::ScreenHeight - 40, L"ESC 타이틀로    F9 한 컴퓨터 2인 모드");
}
