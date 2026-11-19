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

    // 서버 없이 로비 화면을 확인하는 미리보기 (로그인 화면 F8). 샘플 방 목록과 전적을 채운다
    static void StartLobbyPreview();
    static void EndLobbyPreview() { sLobbyPreview = false; }
    static bool IsLobbyPreview() { return sLobbyPreview; }

    // ---- 방 입장 · 방 상태 ----
    struct RoomPlayerView {
        bool         exist = false;   // false = 빈 자리
        int          id = -1;
        std::wstring name;
        int          color = 0;       // 0~6
        bool         ready = false;
    };
    struct RoomState {
        int            roomId = 0;
        MAP_ID         map = MAP_FOREST;
        int            hostSlot = 0;
        RoomPlayerView players[MAX_ROOM_PLAYER];
    };
    enum class JoinState { None, Waiting, Joined, Failed };
    // C2S_JoinRoom을 보내고 S2C_RoomEnterResult를 기다린다 (미리보기면 샘플 방에 바로 들어감)
    static void RequestJoinRoom(int roomId);
    static JoinState GetJoinState() { return sJoinState; }
    static const std::wstring& JoinFailReason() { return sJoinFailReason; }
    // 실패 안내를 한 번 보여 준 뒤 None으로
    static void ClearJoinFailure() { if (sJoinState == JoinState::Failed) sJoinState = JoinState::None; }
    static const RoomState& CurrentRoom() { return sRoom; }
    static int MySlot() { return sMySlot; }
    static bool AmIHost() { return sMySlot >= 0 && sRoom.hostSlot == sMySlot; }
    // C2S_LeaveRoom을 보내고 방 상태를 비운다
    static void LeaveRoom();
    // 방 안에서 보내기. 결과는 서버가 S2C_RoomInfo로 다시 알려 준다 (미리보기면 바로 반영)
    static void SelectMap(MAP_ID map);   // 방장만. 바꾸면 모두의 준비가 풀린다
    static void SelectColor(int color);  // 0~6, 다른 사람이 쓰는 색은 안 됨
    static void SetReady(bool ready);
    static void StartGame();             // 방장만

    // ---- 게임 시작 (S2C_GameStart) ----
    struct StartPlayerView {
        bool         exist = false;   // false = 빈 자리
        int          id = -1;
        std::wstring name;            // 대기방 정보에서 찾은 닉네임
        int          col = 0, row = 0;  // 시작 칸 (서버가 정함: 맵 네 모서리)
        int          color = 0;
    };
    struct GameStartInfo {
        MAP_ID          map = MAP_FOREST;
        int             mySlot = -1;
        StartPlayerView players[MAX_ROOM_PLAYER];
    };
    // 서버가 게임 시작을 알려 왔는지 (방을 나가거나 끊기면 false)
    static bool IsInGame() { return sInGame; }
    static const GameStartInfo& GameStart() { return sGameStart; }

    // ---- 채팅 ----
    // 로비 채팅 = 접속한 모든 사람, 방 채팅 = 같은 방 사람만. 안내 문구([알림])도 같은 칸에 쌓인다.
    // 아직 서버 연동 전이라 내 화면에만 쌓인다
    enum class ChatChannel { Lobby, Room };
    struct ChatLine { std::wstring text; COLORREF color; };
    static void AddChat(ChatChannel channel, const std::wstring& text, COLORREF color);
    static const std::vector<ChatLine>& Chat(ChatChannel channel);
    static void ClearChat(ChatChannel channel);
    // 내가 쓴 채팅. TODO: 서버 연동 (지금은 내 화면에만 '닉네임 : 내용'으로 쌓는다)
    static void SendChat(ChatChannel channel, const std::wstring& text);

    // ---- S2C 반영 ----
    static void OnEcho(const S2C_Echo* p);
    static void OnLoginResult(const S2C_LoginResult* p);
    static void OnRoomList(const S2C_RoomList* p);
    static void OnRoomEnterResult(const S2C_RoomEnterResult* p);
    static void OnRoomInfo(const S2C_RoomInfo* p);
    static void OnPlayerLeave(const S2C_PlayerLeave* p);
    static void OnMapInfo(const S2C_MapInfo* packet);
    static void OnAttack(const S2C_Attack* packet);
    static void OnGameStart(const S2C_GameStart* p);

private:
    static void BuildPreviewRoom(int roomId);
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
    static bool sLobbyPreview;

    static JoinState sJoinState;
    static std::wstring sJoinFailReason;
    static RoomState sRoom;
    static int sMySlot;

    static bool sInGame;
    static GameStartInfo sGameStart;

    static std::vector<ChatLine> sLobbyChat;
    static std::vector<ChatLine> sRoomChat;

    static std::wstring sNotice;
    static ULONGLONG sNoticeUntil;

    // 1단계 에코 테스트 (F5 접속, F6 전송)
    static bool sEchoTestVisible;
    static int sEchoSent;
    static int sEchoReceived;
    static std::wstring sEchoStatus;
    static std::wstring sLastEcho;
};
