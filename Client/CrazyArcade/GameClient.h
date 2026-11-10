#pragma once
#include "Common.h"
#include "protocol.h"

// 클라이언트의 온라인 상태(접속·내 정보·안내 문구)와 서버 패킷 반영을 맡는다.
// NetworkManager가 패킷을 받으면 GameClient::On~를 부르고,
// 화면(씬)에서 서버로 보낼 일이 생기면 NetworkManager::Send~를 부른다.
class GameClient {
public:
    // 매 프레임 씬 Update 전에: 패킷 받기/보내기, 끊김 처리
    static void Update();
    // 씬을 그린 뒤 맨 위에: 안내 띠, 에코 테스트 표시
    static void RenderOverlay(HDC hdc);

    // 화면 가운데에 몇 초간 안내 문구
    static void ShowNotice(const std::wstring& text);

    // ---- S2C 반영 ----
    static void OnEcho(const S2C_Echo* p);

private:
    static void UpdateEchoTest();
    static void RenderEchoTest(HDC hdc);

    static std::wstring sNotice;
    static ULONGLONG sNoticeUntil;

    // 1단계 에코 테스트 (F5 접속, F6 전송)
    static bool sEchoTestVisible;
    static int sEchoSent;
    static int sEchoReceived;
    static std::wstring sEchoStatus;
    static std::wstring sLastEcho;
};
