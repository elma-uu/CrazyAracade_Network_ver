#pragma once
#include "Common.h"
#include "protocol.h"

// 서버 패킷을 받아 클라 상태에 반영한다
class GameClient {
public:
    // 매 프레임 패킷 처리
    static void Update();
    // 안내 문구 그리기
    static void RenderOverlay(HDC hdc);

    // 화면 가운데에 몇 초간 안내 문구
    static void ShowNotice(const std::wstring& text);

    // 내 정보
    static void SetLoginInfo(const std::wstring& name, const std::wstring& serverIp);
    static const std::wstring& MyName() { return sMyName; }
    static const std::wstring& ServerIp() { return sServerIp; }
    // C2S_Login에 넣을 UTF-8 닉네임
    static std::string MyNameUtf8();

    // 로그인
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

    // 로비 방 목록
    struct RoomView {
        int            id = 0;              // 1~8
        MAP_ID         map = MAP_FOREST;
        int            players = 0;         // 0~MAX_ROOM_PLAYER
        ROOM_STATE_NET state = ROOM_EMPTY;
        std::wstring   host;
    };
    // 방 목록 요청
    static void RequestRoomList();
    static const RoomView& Room(int index) { return sRooms[index]; }   // index 0~MAX_ROOM-1
    static bool HasRoomList() { return sHasRoomList; }
    // 이전 목록 지우기
    static void ClearRoomList();

    // 서버 없이 로비 미리보기 (F8)
    static void StartLobbyPreview();
    static void EndLobbyPreview() { sLobbyPreview = false; }
    static bool IsLobbyPreview() { return sLobbyPreview; }

    // 방 입장, 방 상태
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
    // 방 입장 요청
    static void RequestJoinRoom(int roomId);
    static JoinState GetJoinState() { return sJoinState; }
    static const std::wstring& JoinFailReason() { return sJoinFailReason; }
    // 실패 안내를 한 번 보여 준 뒤 None으로
    static void ClearJoinFailure() { if (sJoinState == JoinState::Failed) sJoinState = JoinState::None; }
    static const RoomState& CurrentRoom() { return sRoom; }
    static int MySlot() { return sMySlot; }
    static bool AmIHost() { return sMySlot >= 0 && sRoom.hostSlot == sMySlot; }
    // 방 나가기
    static void LeaveRoom();
    // 결과는 S2C_RoomInfo로 온다
    static void SelectMap(MAP_ID map);   // 방장만. 바꾸면 모두의 준비가 풀린다
    static void SelectColor(int color);  // 0~6, 다른 사람이 쓰는 색은 안 됨
    static void SetReady(bool ready);
    static void StartGame();             // 방장만

    // 게임 시작 정보
    struct StartPlayerView {
        bool         exist = false;   // false = 빈 자리
        int          id = -1;
        std::wstring name;            // 대기방 정보에서 찾은 닉네임
        int          col = 0, row = 0;  // 시작 칸
        int          color = 0;
    };
    struct GameStartInfo {
        MAP_ID          map = MAP_FOREST;
        int             mySlot = -1;
        StartPlayerView players[MAX_ROOM_PLAYER];
    };
    // 게임 중인지
    static bool IsInGame() { return sInGame; }
    static const GameStartInfo& GameStart() { return sGameStart; }

    // 채팅
    // 아직 서버와 연결 안 돼서 내 화면에만 보인다
    enum class ChatChannel { Lobby, Room };
    struct ChatLine { std::wstring text; COLORREF color; };
    static void AddChat(ChatChannel channel, const std::wstring& text, COLORREF color);
    static const std::vector<ChatLine>& Chat(ChatChannel channel);
    static void ClearChat(ChatChannel channel);
    // TODO: 서버 연동
    static void SendChat(ChatChannel channel, const std::wstring& text);

    // 서버 패킷 처리
    static void OnEcho(const S2C_Echo* p);
    static void OnLoginResult(const S2C_LoginResult* p);
    static void OnRoomList(const S2C_RoomList* p);
    static void OnRoomEnterResult(const S2C_RoomEnterResult* p);
    static void OnRoomInfo(const S2C_RoomInfo* p);
    static void OnPlayerLeave(const S2C_PlayerLeave* p);
    static void OnMapInfo(const S2C_MapInfo* packet);
    static void OnBlockBreak(const S2C_BlockBreak* packet);
    static void OnExplode(const S2C_Explode* packet);
    static void OnAttack(const S2C_Attack* packet);
    static void OnGameStart(const S2C_GameStart* p);

    // 서버가 보낸 이동
    struct MoveView {
        int   x = 0, y = 0;           // 출발 칸
        int   destX = 0, destY = 0;   // 목적 칸
        int   dir = IN_DOWN;          // INPUT_DIR
        float speed = 1.0f;
    };
    static void OnMove(const S2C_Move* p);
    // 새 이동이 있으면 꺼내 준다
    static bool TakeMove(int slot, MoveView& out);

    // 스테이지 상태 (사과·사과상자·하트·시간·생존)
    struct StageView {
        int  apple = 0, appleBox = 0, heart = 0;
        int  timeLeft = 0;
        bool alive = true;
    };
    static void OnStageState(const S2C_StageState* p);
    static const StageView& Stage(int slot) { return sStage[slot]; }
    static bool HasStageState() { return sHasStage; }

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

    static MoveView sMoves[MAX_ROOM_PLAYER];
    static bool sHasMove[MAX_ROOM_PLAYER];

    static StageView sStage[MAX_ROOM_PLAYER];
    static bool sHasStage;

    static std::vector<ChatLine> sLobbyChat;
    static std::vector<ChatLine> sRoomChat;

    static std::wstring sNotice;
    static ULONGLONG sNoticeUntil;

    // 에코 테스트 (F5, F6)
    static bool sEchoTestVisible;
    static int sEchoSent;
    static int sEchoReceived;
    static std::wstring sEchoStatus;
    static std::wstring sLastEcho;
};
