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

    // ---- 내 정보 (로그인 화면에서 입력, 다시 들어오면 그대로 채워 줌) ----
    static void SetLoginInfo(const std::wstring& name, const std::wstring& serverIp);
    static const std::wstring& MyName() { return sMyName; }
    static const std::wstring& ServerIp() { return sServerIp; }
    // C2S_Login에 넣을 UTF-8 닉네임
    static std::string MyNameUtf8();

    // ---- 로그인 ----
    enum class LoginState { None, Waiting, LoggedIn, Failed };
    // C2S_Login을 보내고 응답을 기다리기 시작
    static void RequestLogin();
    static LoginState GetLoginState() { return sLoginState; }
    static ULONGLONG LoginRequestedAt() { return sLoginRequestedAt; }
    // 연결이 끊기거나 로그인 화면을 떠날 때
    static void ResetLogin();
    static int MyId() { return sMyId; }
    static int Wins() { return sWins; }
    static int Losses() { return sLosses; }

    // ---- 로비: 방 목록 ----
    struct RoomView {
        int            id = 0;              // 1~8
        MAP_ID         map = MAP_FOREST;
        int            players = 0;         // 0~MAX_ROOM_PLAYER
        ROOM_STATE_NET state = ROOM_EMPTY;
        std::wstring   host;
    };
    // C2S_RoomList를 보낸다. 응답이 오면 Room(i)가 갱신된다
    static void RequestRoomList();
    static const RoomView& Room(int index) { return sRooms[index]; }   // index 0~MAX_ROOM-1
    static bool HasRoomList() { return sHasRoomList; }
    // 로비에 들어갈 때 이전 목록이 잠깐 보이지 않게
    static void ClearRoomList();

    // ---- S2C 반영 ----
    static void OnEcho(const S2C_Echo* p);
    static void OnLoginResult(const S2C_LoginResult* p);
    static void OnRoomList(const S2C_RoomList* p);

private:
    static void UpdateEchoTest();
    static void RenderEchoTest(HDC hdc);

    static std::wstring sMyName;
    static std::wstring sServerIp;

    static LoginState sLoginState;
    static ULONGLONG sLoginRequestedAt;
    static int sMyId;
    static int sWins;
    static int sLosses;

    static RoomView sRooms[MAX_ROOM];
    static bool sHasRoomList;

    static std::wstring sNotice;
    static ULONGLONG sNoticeUntil;

    // 1단계 에코 테스트 (F5 접속, F6 전송)
    static bool sEchoTestVisible;
    static int sEchoSent;
    static int sEchoReceived;
    static std::wstring sEchoStatus;
    static std::wstring sLastEcho;
};
