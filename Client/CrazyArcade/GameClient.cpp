#include "GameClient.h"
#include "SceneManager.h"
#include "NetworkManager.h"
#include "Input.h"
#include "Gfx.h"
#include <cstdio>

std::wstring GameClient::sMyName;
std::wstring GameClient::sServerIp = L"127.0.0.1";
GameClient::LoginState GameClient::sLoginState = GameClient::LoginState::None;
ULONGLONG GameClient::sLoginRequestedAt = 0;
int GameClient::sMyId = -1;
int GameClient::sWins = 0;
int GameClient::sLosses = 0;
GameClient::RoomView GameClient::sRooms[MAX_ROOM];
bool GameClient::sHasRoomList = false;
bool GameClient::sLobbyPreview = false;
GameClient::JoinState GameClient::sJoinState = GameClient::JoinState::None;
std::wstring GameClient::sJoinFailReason;
GameClient::RoomState GameClient::sRoom;
int GameClient::sMySlot = -1;
bool GameClient::sInGame = false;
GameClient::GameStartInfo GameClient::sGameStart;
GameClient::MoveView GameClient::sMoves[MAX_ROOM_PLAYER];
bool GameClient::sHasMove[MAX_ROOM_PLAYER] = {};
std::vector<GameClient::ChatLine> GameClient::sLobbyChat;
std::vector<GameClient::ChatLine> GameClient::sRoomChat;
std::wstring GameClient::sNotice;
ULONGLONG GameClient::sNoticeUntil = 0;
bool GameClient::sEchoTestVisible = false;
int GameClient::sEchoSent = 0;
int GameClient::sEchoReceived = 0;
std::wstring GameClient::sEchoStatus;
std::wstring GameClient::sLastEcho;

namespace {
    constexpr ULONGLONG kNoticeMs = 3000;
    constexpr int kColorCount = 7;   // 색 0~6

    // UTF-8 이름을 화면용 wstring으로 바꾼다
    std::wstring Utf8NameToWide(const char* name, size_t maxLen)
    {
        int bytes = static_cast<int>(strnlen(name, maxLen));
        if (bytes == 0) return {};
        int chars = MultiByteToWideChar(CP_UTF8, 0, name, bytes, nullptr, 0);
        std::wstring wide(chars, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, name, bytes, wide.data(), chars);
        return wide;
    }
}

void GameClient::Update()
{
    UpdateEchoTest();

    // 서버 패킷을 먼저 처리한다
    NetworkManager::Poll();
    if (NetworkManager::ConsumeConnectionLost()) {
        ResetLogin();
        ShowNotice(L"연결 끊김: " + NetworkManager::LastError());
    }
}

void GameClient::RenderOverlay(HDC hdc)
{
    RenderEchoTest(hdc);
    if (GetTickCount64() < sNoticeUntil) Gfx::DrawNoticeBar(hdc, sNotice);
}

void GameClient::ShowNotice(const std::wstring& text)
{
    sNotice = text;
    sNoticeUntil = GetTickCount64() + kNoticeMs;
}

void GameClient::SetLoginInfo(const std::wstring& name, const std::wstring& serverIp)
{
    sMyName = name;
    sServerIp = serverIp;
}

std::string GameClient::MyNameUtf8()
{
    if (sMyName.empty()) return {};
    int bytes = WideCharToMultiByte(CP_UTF8, 0, sMyName.c_str(), static_cast<int>(sMyName.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(bytes, '\0');
    WideCharToMultiByte(CP_UTF8, 0, sMyName.c_str(), static_cast<int>(sMyName.size()), utf8.data(), bytes, nullptr, nullptr);
    return utf8;
}

void GameClient::RequestLogin()
{
    NetworkManager::SendLogin(MyNameUtf8());
    sLoginState = LoginState::Waiting;
    sLoginRequestedAt = GetTickCount64();
}

void GameClient::ResetLogin()
{
    sLoginState = LoginState::None;
    sMyId = -1;
    sWins = sLosses = 0;   // 이전 전적 지우기
    // 방 상태도 비운다
    sRoom = RoomState{};
    sMySlot = -1;
    sJoinState = JoinState::None;
    sInGame = false;
    sGameStart = GameStartInfo{};
    ClearChat(ChatChannel::Room);
    ClearChat(ChatChannel::Lobby);   // 로그아웃하면 로비 채팅도 비운다
}

void GameClient::OnLoginResult(const S2C_LoginResult* p)
{
    // 요청하지 않은 응답은 무시
    if (sLoginState != LoginState::Waiting) return;

    if (!p->success) {
        sLoginState = LoginState::Failed;
        return;
    }
    sLoginState = LoginState::LoggedIn;
    sMyId = p->id;
    sWins = p->win;
    sLosses = p->lose;
}

void GameClient::RequestRoomList()
{
    NetworkManager::SendRoomList();
}

void GameClient::ClearRoomList()
{
    for (int i = 0; i < MAX_ROOM; ++i) {
        sRooms[i] = RoomView{};
        sRooms[i].id = i + 1;
    }
    sHasRoomList = false;
}

void GameClient::StartLobbyPreview()
{
    sLobbyPreview = true;
    if (sMyName.empty()) sMyName = L"미리보기";
    sWins = 12;
    sLosses = 7;

    struct Sample { MAP_ID map; int players; ROOM_STATE_NET state; const wchar_t* host; };
    static const Sample samples[MAX_ROOM] = {
        { MAP_FOREST, 4, ROOM_WAITING, L"지은" },
        { MAP_ICE,    4, ROOM_PLAYING, L"규동" },
        { MAP_CITY,   1, ROOM_WAITING, L"민수" },
        { MAP_FOREST, 2, ROOM_WAITING, L"형민" },
        { MAP_FOREST, 0, ROOM_EMPTY,   L"" },
        { MAP_ICE,    3, ROOM_PLAYING, L"서연" },
        { MAP_FOREST, 0, ROOM_EMPTY,   L"" },
        { MAP_CITY,   2, ROOM_WAITING, L"하늘" },
    };
    for (int i = 0; i < MAX_ROOM; ++i) {
        sRooms[i] = { i + 1, samples[i].map, samples[i].players, samples[i].state, samples[i].host };
    }
    sHasRoomList = true;
}

void GameClient::OnRoomList(const S2C_RoomList* p)
{
    for (int i = 0; i < MAX_ROOM; ++i) {
        const ROOM_SUMMARY& s = p->rooms[i];
        RoomView& r = sRooms[i];
        r.id = s.roomId;
        r.map = s.mapId;
        r.players = std::min<int>(s.playerCount, MAX_ROOM_PLAYER);
        r.state = s.state;
        r.host = Utf8NameToWide(s.hostName, sizeof(s.hostName));
    }
    sHasRoomList = true;
}

void GameClient::AddChat(ChatChannel channel, const std::wstring& text, COLORREF color)
{
    std::vector<ChatLine>& log = (channel == ChatChannel::Lobby) ? sLobbyChat : sRoomChat;
    log.push_back({ text, color });
    constexpr size_t kMaxLines = 200;   // 오래된 줄부터 버린다
    if (log.size() > kMaxLines) log.erase(log.begin(), log.begin() + (log.size() - kMaxLines));
}

const std::vector<GameClient::ChatLine>& GameClient::Chat(ChatChannel channel)
{
    return (channel == ChatChannel::Lobby) ? sLobbyChat : sRoomChat;
}

void GameClient::SendChat(ChatChannel channel, const std::wstring& text)
{
    // TODO: 채팅 패킷이 생기면 서버로 보내기
    AddChat(channel, sMyName + L" : " + text, RGB(255, 255, 255));
}

void GameClient::ClearChat(ChatChannel channel)
{
    ((channel == ChatChannel::Lobby) ? sLobbyChat : sRoomChat).clear();
}

void GameClient::RequestJoinRoom(int roomId)
{
    sJoinFailReason.clear();
    if (sLobbyPreview) { BuildPreviewRoom(roomId); return; }
    sJoinState = JoinState::Waiting;
    NetworkManager::SendJoinRoom(roomId);
}

void GameClient::BuildPreviewRoom(int roomId)
{
    // 미리보기용 가짜 방 상태
    const RoomView& r = sRooms[std::clamp(roomId - 1, 0, MAX_ROOM - 1)];
    sRoom = RoomState{};
    sRoom.roomId = roomId;
    sRoom.map = (r.players > 0) ? r.map : MAP_FOREST;
    sRoom.hostSlot = 0;

    static const wchar_t* const others[] = { L"규동", L"형민", L"서연", L"민수", L"하늘", L"지은" };
    int slot = 0, other = 0;
    for (int i = 0; i < r.players && slot < MAX_ROOM_PLAYER - 1; ++i, ++slot) {
        RoomPlayerView& p = sRoom.players[slot];
        p.exist = true;
        p.id = 1000 + slot;
        if (i == 0) {
            p.name = r.host;   // 첫 자리가 방장
        } else {
            while (r.host == others[other % 6]) ++other;
            p.name = others[other++ % 6];
        }
        p.color = (slot * 3) % kColorCount;   // 서로 다른 색
        p.ready = (i != 0) && (i % 2 == 1);  // 방장 빼고 일부만 준비
    }

    // 빈자리에 나를 넣고 안 쓰는 색을 준다
    RoomPlayerView& me = sRoom.players[slot];
    me.exist = true;
    me.id = (sMyId >= 0) ? sMyId : 1;
    me.name = sMyName;
    for (int color = 0; color < kColorCount; ++color) {
        bool used = false;
        for (int i = 0; i < slot; ++i) used |= sRoom.players[i].color == color;
        if (!used) { me.color = color; break; }
    }
    sMySlot = slot;
    sJoinState = JoinState::Joined;
    ClearChat(ChatChannel::Room);
}

void GameClient::LeaveRoom()
{
    if (sJoinState == JoinState::Joined) NetworkManager::SendLeaveRoom();
    sRoom = RoomState{};
    sMySlot = -1;
    sJoinState = JoinState::None;
    sInGame = false;
    sGameStart = GameStartInfo{};
    ClearChat(ChatChannel::Room);   // 방 채팅은 그 방에 있는 동안만
}

void GameClient::SelectMap(MAP_ID map)
{
    if (!AmIHost()) return;
    if (!sLobbyPreview) { NetworkManager::SendSelectMap(map); return; }
    sRoom.map = map;
    for (RoomPlayerView& p : sRoom.players) p.ready = false;   // 맵이 바뀌면 준비 해제
}

void GameClient::SelectColor(int color)
{
    if (sMySlot < 0 || color < 0 || color >= kColorCount) return;
    if (!sLobbyPreview) { NetworkManager::SendSelectColor(color); return; }
    for (int i = 0; i < MAX_ROOM_PLAYER; ++i)
        if (i != sMySlot && sRoom.players[i].exist && sRoom.players[i].color == color) return;
    sRoom.players[sMySlot].color = color;
}

void GameClient::SetReady(bool ready)
{
    if (sMySlot < 0 || AmIHost()) return;
    if (!sLobbyPreview) { NetworkManager::SendReady(ready); return; }
    sRoom.players[sMySlot].ready = ready;
}

void GameClient::StartGame()
{
    if (!AmIHost()) return;
    if (!sLobbyPreview) {
        // 서버는 방장도 레디해야 시작하므로 레디를 먼저 보낸다
        NetworkManager::SendReady(true);
        NetworkManager::SendStartGame();
        return;
    }
    ShowNotice(L"미리보기에서는 게임을 시작하지 않아요");
}

void GameClient::OnRoomEnterResult(const S2C_RoomEnterResult* p)
{
    if (sJoinState != JoinState::Waiting) return;   // 요청하지 않은 응답
    if (!p->success) {
        sJoinState = JoinState::Failed;
        sJoinFailReason = (p->failReason == ENTER_PLAYING) ? L"이미 게임 중인 방이에요"
            : (p->failReason == ENTER_FULL) ? L"방이 꽉 찼어요" : L"방에 들어가지 못했어요";
        return;
    }
    sJoinState = JoinState::Joined;
    ClearChat(ChatChannel::Room);
    sRoom = RoomState{};
    sRoom.roomId = p->roomId;
    sMySlot = p->slot;
    // 나머지는 S2C_RoomInfo로 채워진다
}

void GameClient::OnRoomInfo(const S2C_RoomInfo* p)
{
    if (sJoinState != JoinState::Joined) return;
    sRoom.roomId = p->roomId;
    sRoom.map = p->mapId;
    sRoom.hostSlot = p->hostSlot;
    for (int i = 0; i < MAX_ROOM_PLAYER; ++i) {
        const ROOM_PLAYER& s = p->players[i];
        RoomPlayerView& v = sRoom.players[i];
        v.exist = s.exist;
        v.id = s.id;
        v.name = Utf8NameToWide(s.name, sizeof(s.name));
        v.color = s.color;
        v.ready = s.ready;
    }
}

void GameClient::OnPlayerLeave(const S2C_PlayerLeave* p)
{
    if (sJoinState != JoinState::Joined) return;
    for (RoomPlayerView& v : sRoom.players)
        if (v.exist && v.id == p->id) v = RoomPlayerView{};
    sRoom.hostSlot = p->newHostSlot;
}

void GameClient::OnGameStart(const S2C_GameStart* p)
{
    // 방에 있을 때만 받는다
    if (sJoinState != JoinState::Joined) return;
    if (p->mapId < MAP_FOREST || p->mapId > MAP_ICE || p->mySlot >= MAX_ROOM_PLAYER) {
        OutputDebugStringA("[net] S2C_GameStart: 잘못된 맵/자리, 무시\n");
        return;
    }

    sGameStart = GameStartInfo{};
    sGameStart.map = p->mapId;
    sGameStart.mySlot = p->mySlot;
    for (int i = 0; i < MAX_ROOM_PLAYER; ++i) {
        const START_PLAYER& s = p->players[i];
        if (s.id < 0) continue;   // 빈 자리
        StartPlayerView& v = sGameStart.players[i];
        v.exist = true;
        v.id = s.id;
        v.col = std::min<int>(s.x, MAP_TILES - 1);
        v.row = std::min<int>(s.y, MAP_TILES - 1);
        v.color = std::min<int>(s.color, kColorCount - 1);
        for (const RoomPlayerView& rp : sRoom.players)   // 닉네임은 방 정보에서
            if (rp.exist && rp.id == s.id) v.name = rp.name;
    }
    sMySlot = p->mySlot;
    sInGame = true;
    for (int i = 0; i < MAX_ROOM_PLAYER; ++i) sHasMove[i] = false;   // 지난 판 이동이 남지 않게

    // 바로 뒤에 오는 S2C_MapInfo를 받을 수 있게 지금 스테이지 전환을 예약한다.
    // (RoomScene도 다음 프레임에 같은 전환을 부르지만, ChangeScene이 같은 씬 예약은 무시하므로
    //  받아 둔 맵 정보가 지워지지 않는다)
    switch (p->mapId) {
    case MAP_CITY: SceneManager::ChangeScene(SceneType::City); break;
    case MAP_ICE: SceneManager::ChangeScene(SceneType::Ice); break;
    default: SceneManager::ChangeScene(SceneType::Forest); break;
    }
}

// id를 자리 번호로 바꿔 저장해 둔다
void GameClient::OnMove(const S2C_Move* p)
{
    if (!sInGame) return;
    if (p->x >= MAP_TILES || p->y >= MAP_TILES || p->destX >= MAP_TILES || p->destY >= MAP_TILES) return;

    int slot = -1;
    for (int i = 0; i < MAX_ROOM_PLAYER; ++i)
        if (sGameStart.players[i].exist && sGameStart.players[i].id == p->id) slot = i;
    if (slot == -1) return;   // 모르는 사람

    MoveView& m = sMoves[slot];
    m.x = p->x;
    m.y = p->y;
    m.destX = p->destX;
    m.destY = p->destY;
    m.dir = p->dir;
    m.speed = p->speed;
    sHasMove[slot] = true;   // 여러 개 오면 마지막 것만 쓴다
}

bool GameClient::TakeMove(int slot, MoveView& out)
{
    if (slot < 0 || slot >= MAX_ROOM_PLAYER || !sHasMove[slot]) return false;
    out = sMoves[slot];
    sHasMove[slot] = false;
    return true;
}

void GameClient::OnEcho(const S2C_Echo* p)
{
    std::string msg(p->message, strnlen(p->message, sizeof(p->message)));
    sLastEcho.assign(msg.begin(), msg.end());   // 에코 테스트 문자열은 ASCII
    ++sEchoReceived;
}

void GameClient::UpdateEchoTest()
{
    if (Input::GetKeyDown(VK_F5)) {
        sEchoTestVisible = true;
        sEchoSent = sEchoReceived = 0;
        sLastEcho.clear();
        if (NetworkManager::Connect("127.0.0.1", SERVER_PORT)) {
            sEchoStatus = L"접속 성공";
        } else {
            sEchoStatus = L"접속 실패";
            ShowNotice(L"접속 실패: " + NetworkManager::LastError());
        }
    }
    if (Input::GetKeyDown(VK_F6) && NetworkManager::IsConnected()) {
        char msg[64];
        sprintf_s(msg, "echo test #%d", ++sEchoSent);
        NetworkManager::SendEcho(msg);
    }
}

void GameClient::RenderEchoTest(HDC hdc)
{
    if (!sEchoTestVisible) return;
    Gfx::DrawSmallText(hdc, 10, 10, L"[F5] 접속  [F6] 에코  |  " + sEchoStatus +
        (NetworkManager::IsConnected() ? L" (연결됨)" : L" (연결 안 됨)"));
    Gfx::DrawSmallText(hdc, 10, 36, L"보냄 " + std::to_wstring(sEchoSent) +
        L" / 받음 " + std::to_wstring(sEchoReceived) + L"  마지막: " + sLastEcho);
}

void GameClient::OnMapInfo(const S2C_MapInfo* packet)
{
    if (!sInGame || packet->mapId != sGameStart.map) return;
    SceneManager::OnMapInfo(*packet);
}

void GameClient::OnAttack(const S2C_Attack* packet)
{
    if (!sInGame) return;
    SceneManager::OnAttack(*packet);
}

void GameClient::OnExplode(const S2C_Explode* packet)
{
    if (sInGame) SceneManager::OnExplode(*packet);
}

void GameClient::OnBlockBreak(const S2C_BlockBreak* packet)
{
    if (sInGame) SceneManager::OnBlockBreak(*packet);
}
