#pragma once
#include "Scene.h"

// 로그인: 닉네임과 서버 IP를 입력하고 서버에 접속한다.
// Tab/클릭 = 입력칸 이동, Enter = 확인, ESC = 타이틀로, F9 = 기존 한 컴퓨터 2인 모드
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
    // 로그인 요청 후: 성공/실패/시간 초과/끊김 처리
    void UpdateLoginWait();

    std::wstring mName;
    std::wstring mIp = L"127.0.0.1";
    int mFocus = FieldName;
    std::wstring mMessage;           // 입력 오류·접속 결과 안내
    COLORREF mMessageColor = RGB(255, 120, 120);
    // Enter 직후 한 프레임은 "접속 중..."을 그리고, 다음 프레임에 접속한다 (접속은 최대 4초 멈춤)
    bool mConnectPending = false;
    bool mConnected = false;   // 접속 성공, 로그인 요청 보냄 (응답 대기 또는 성공)
};
