#pragma once
#include "Scene.h"

// 로그인 화면
class LoginScene : public Scene {
public:
    void OnEnter() override;
    void OnExit() override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;

private:
    enum Field { FieldName, FieldIp, FieldCount };

    void HandleClick();
    void HandleTyping();
    void TypeChar(wchar_t ch);
    void Submit();
    void ConnectToServer();
    // 로그인 응답 기다리기
    void UpdateLoginWait();

    std::wstring mName;
    std::wstring mIp = L"127.0.0.1";
    int mFocus = FieldName;
    std::wstring mMessage;           // 입력 오류·접속 결과 안내
    COLORREF mMessageColor = RGB(255, 120, 120);
    // 접속 중 문구를 먼저 그리고 다음 프레임에 접속
    bool mConnectPending = false;
    bool mConnected = false;   // 접속해서 로그인 요청 보냄
};
