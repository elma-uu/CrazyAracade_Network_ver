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

    std::wstring mName;
    std::wstring mIp = L"127.0.0.1";
    int mFocus = FieldName;
    std::wstring mMessage;   // 입력 오류 안내
};
