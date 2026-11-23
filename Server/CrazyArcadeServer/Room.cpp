#include "Room.h"
#include "Session.h"
#include <cstring>
#include <iostream>

RoomManager g_roomManager;

// ---------------------------------------------------------------- Room

void Room::Init(int id)
{
    m_id = id;
}

bool Room::Enter(int sessionId, int& outSlot, unsigned char& failReason)
{
    if (int slot = FindSlot(sessionId); slot != -1) {   // 이미 이 방에 있음
        outSlot = slot;
        return true;
    }
    if (m_state == RS_PLAYING) {
        failReason = ENTER_PLAYING;
        return false;
    }

    int slot = -1;
    for (int i = 0; i < MAX_ROOM_PLAYER; ++i) {
        if (m_members[i] == -1) { slot = i; break; }
    }
    if (slot == -1) {
        failReason = ENTER_FULL;
        return false;
    }

    m_members[slot]    = sessionId;
    m_color[slot]      = static_cast<unsigned char>(slot);   // 기본 색상은 자리 번호
    m_ready[slot]      = false;
    m_enterOrder[slot] = m_nextOrder++;

    if (m_state == RS_EMPTY) {          // 빈 방에 처음 들어온 사람이 방장
        m_state    = RS_WAITING;
        m_hostSlot = slot;
    }

    outSlot = slot;
    return true;
}

void Room::Leave(int sessionId)
{
    int slot = FindSlot(sessionId);
    if (slot == -1)
        return;

    m_members[slot] = -1;
    m_ready[slot]   = false;

    if (GetPlayerCount() == 0) {        // 마지막 사람이 나감 → 빈 방으로 초기화
        m_state     = RS_EMPTY;
        m_hostSlot  = -1;
        m_mapId     = MAP_FOREST;
        m_nextOrder = 0;
        return;
    }

    if (slot == m_hostSlot) {           // 방장이 나감 → 가장 먼저 들어온 사람에게 위임
        int next = -1;
        for (int i = 0; i < MAX_ROOM_PLAYER; ++i) {
            if (m_members[i] == -1) continue;
            if (next == -1 || m_enterOrder[i] < m_enterOrder[next])
                next = i;
        }
        m_hostSlot = next;
        std::cout << "[room " << m_id << "] host -> slot " << next
                  << " (id " << m_members[next] << ")" << std::endl;
    }

    // 남은 사람들에게: 누가 나갔는지 + 지금 방장 자리 → 바뀐 방 정보
    S2C_PlayerLeave leave{};
    leave.size        = sizeof(leave);
    leave.type        = S2C_PLAYER_LEAVE;
    leave.id          = sessionId;
    leave.newHostSlot = static_cast<unsigned char>(m_hostSlot);
    Broadcast(&leave);
    SendRoomInfo();
}

bool Room::IsHost(int sessionId) const
{
    return m_hostSlot != -1 && m_members[m_hostSlot] == sessionId;
}

int Room::FindSlot(int sessionId) const
{
    for (int i = 0; i < MAX_ROOM_PLAYER; ++i)
        if (m_members[i] == sessionId)
            return i;
    return -1;
}

int Room::GetPlayerCount() const
{
    int count = 0;
    for (int id : m_members)
        if (id != -1)
            ++count;
    return count;
}

void Room::Broadcast(const void* packet, int exceptId)
{
    for (int id : m_members)
        if (id != -1 && id != exceptId)
            g_sessions[id].SendPacket(packet);
}

void Room::SendRoomInfo()
{
    S2C_RoomInfo info;
    FillRoomInfo(info);
    Broadcast(&info);
}

bool Room::SelectMap(int sessionId, int mapId)
{
    const char* reason = nullptr;
    if (m_state != RS_WAITING)                         reason = "not waiting";
    else if (!IsHost(sessionId))                       reason = "not host";
    else if (mapId < MAP_FOREST || mapId > MAP_ICE)    reason = "invalid map";

    if (reason) {
        std::cout << "[room " << m_id << "] select map rejected (id " << sessionId << "): "
                  << reason << std::endl;
        return false;
    }
    if (mapId == m_mapId)
        return true;

    m_mapId = mapId;
    // 바뀐 맵을 확인하고 다시 레디하도록 전원(방장 포함) 레디 해제
    for (bool& ready : m_ready)
        ready = false;
    std::cout << "[room " << m_id << "] map -> " << mapId << " (ready cleared)" << std::endl;
    return true;
}

bool Room::SetColor(int slot, int color)
{
    constexpr int kColorCount = 7;   // 0~6

    const char* reason = nullptr;
    if (m_state != RS_WAITING)                                          reason = "not waiting";
    else if (slot < 0 || slot >= MAX_ROOM_PLAYER || m_members[slot] == -1) reason = "invalid slot";
    else if (color < 0 || color >= kColorCount)                         reason = "invalid color";
    else {
        for (int i = 0; i < MAX_ROOM_PLAYER; ++i)
            if (i != slot && m_members[i] != -1 && m_color[i] == color) { reason = "color in use"; break; }
    }

    if (reason) {
        std::cout << "[room " << m_id << "] select color " << color << " rejected (slot " << slot
                  << "): " << reason << std::endl;
        return false;
    }
    m_color[slot] = static_cast<unsigned char>(color);
    return true;
}

void Room::SetReady(int slot, bool ready)
{
    if (slot < 0 || slot >= MAX_ROOM_PLAYER || m_members[slot] == -1)
        return;
    m_ready[slot] = ready;
}

bool Room::CanStart() const
{
    int count = 0;
    for (int i = 0; i < MAX_ROOM_PLAYER; ++i) {
        if (m_members[i] == -1) continue;   // 빈 자리
        if (!m_ready[i]) return false;      // 레디 안 한 사람 있음 (방장 포함)
        ++count;
    }
    return count >= 2;                      // 혼자서는 시작 불가
}

// SESSION::OnStartGame에서 호출. 클라 화면에서도 막지만 서버가 한 번 더 확인한다.
bool Room::RequestStart(int sessionId)
{
    const char* reason = nullptr;
    if (m_state != RS_WAITING)      reason = "not waiting";
    else if (!IsHost(sessionId))    reason = "not host";
    else if (GetPlayerCount() < 2)  reason = "need 2+ players";
    else if (!CanStart())           reason = "not everyone is ready";

    if (reason) {
        std::cout << "[room " << m_id << "] start rejected (id " << sessionId << "): "
                  << reason << std::endl;
        return false;
    }

    std::cout << "[room " << m_id << "] start approved by host " << sessionId
              << " (" << GetPlayerCount() << " players)" << std::endl;
    return StartGame();
}

// 순서 (계획서 3.5 / 2.5 순서도 ②):
// 1) GameWorld 판 준비 (원본 init_board_*, 네 모서리와 옆 칸은 비워 둠)
// 2) RS_PLAYING — 이때부터 이 방에는 입장 불가(ENTER_PLAYING), 로비 목록에도 "게임 중"
// 3) S2C_GameStart: 받는 사람마다 mySlot이 다르고, 받은 세션은 ST_INGAME (SendGameStart, 김동연)
// 4) S2C_MapInfo: 장애물·종류만 보내고 아이템 위치는 숨김 (BuildMapInfo, 민규동)
bool Room::StartGame()
{
    if (!m_world.InitBoard(static_cast<MAP_ID>(m_mapId))) {
        std::cout << "[room " << m_id << "] start failed: cannot build map " << m_mapId << std::endl;
        return false;
    }
    m_state = RS_PLAYING;

    SendGameStart();

    S2C_MapInfo map{};
    m_world.BuildMapInfo(map);
    Broadcast(&map);

    std::cout << "[room " << m_id << "] game started (map " << m_mapId << ")" << std::endl;
    g_roomManager.BroadcastRoomListToLobby();   // 로비 목록에 "게임 중"으로
    return true;
}

void Room::GetStartTile(int slot, int& x, int& y)
{
    static constexpr int kCorners[MAX_ROOM_PLAYER][2] = {
        { 0, 0 }, { MAP_TILES - 1, 0 }, { 0, MAP_TILES - 1 }, { MAP_TILES - 1, MAP_TILES - 1 },
    };
    const int i = (slot >= 0 && slot < MAX_ROOM_PLAYER) ? slot : 0;
    x = kCorners[i][0];
    y = kCorners[i][1];
}

void Room::SendGameStart()
{
    S2C_GameStart p{};
    p.size  = sizeof(p);
    p.type  = S2C_GAME_START;
    p.mapId = static_cast<MAP_ID>(m_mapId);

    for (int i = 0; i < MAX_ROOM_PLAYER; ++i) {
        START_PLAYER& sp = p.players[i];
        if (m_members[i] == -1) {   // 빈 자리
            sp.id = -1;
            continue;
        }
        int x = 0, y = 0;
        GetStartTile(i, x, y);
        sp.id    = m_members[i];
        sp.x     = static_cast<unsigned char>(x);
        sp.y     = static_cast<unsigned char>(y);
        sp.color = m_color[i];
    }

    // 내용은 같고 mySlot만 받는 사람의 자리로 바꿔 보낸다
    for (int i = 0; i < MAX_ROOM_PLAYER; ++i) {
        if (m_members[i] == -1) continue;
        p.mySlot = static_cast<unsigned char>(i);
        SESSION& s = g_sessions[m_members[i]];
        s.SendPacket(&p);
        s.EnterGame();
    }
    std::cout << "[room " << m_id << "] S2C_GameStart sent (map " << m_mapId << ", "
              << GetPlayerCount() << " players)" << std::endl;
}

void Room::FillRoomInfo(S2C_RoomInfo& out) const
{
    out = {};
    out.size     = sizeof(out);
    out.type     = S2C_ROOM_INFO;
    out.roomId   = static_cast<unsigned char>(m_id);
    out.mapId    = static_cast<MAP_ID>(m_mapId);
    out.hostSlot = static_cast<unsigned char>(m_hostSlot);   // 빈 방이면 0xFF (보낼 일은 없음)

    for (int i = 0; i < MAX_ROOM_PLAYER; ++i) {
        ROOM_PLAYER& p = out.players[i];
        if (m_members[i] == -1) {        // 빈 자리: exist = false, id = -1
            p.id = -1;
            continue;
        }
        p.exist = true;
        p.id    = m_members[i];
        p.color = m_color[i];
        p.ready = m_ready[i];
        memcpy(p.name, g_sessions[m_members[i]].GetName(), MAX_NAME_LEN);
    }
}

// ---------------------------------------------------------------- RoomManager

RoomManager::RoomManager()
{
    for (int i = 0; i < MAX_ROOM; ++i)
        m_rooms[i].Init(i + 1);          // 방 번호는 1~8
}

bool RoomManager::JoinRoom(int sessionId, int roomId, int& outSlot, unsigned char& failReason)
{
    failReason = ENTER_OK;
    if (!IsValidRoomId(roomId))
        return false;
    if (int current = FindRoomOf(sessionId); current != -1 && current != roomId)
        return false;                    // 다른 방에 있으면 먼저 나와야 한다
    return Get(roomId).Enter(sessionId, outSlot, failReason);
}

void RoomManager::LeaveRoom(int sessionId)
{
    if (int roomId = FindRoomOf(sessionId); roomId != -1)
        Get(roomId).Leave(sessionId);
}

int RoomManager::FindRoomOf(int sessionId) const
{
    for (const Room& r : m_rooms)
        if (r.FindSlot(sessionId) != -1)
            return r.GetId();
    return -1;
}

bool RoomManager::CheckConsistency() const
{
    bool ok = true;
    auto fail = [&ok](int roomId, const char* what, int id) {
        std::cout << "[check] room " << roomId << ": " << what << " (id " << id << ")" << std::endl;
        ok = false;
    };

    bool inRoom[MAX_USER]{};
    for (const Room& r : m_rooms) {
        const int count = r.GetPlayerCount();
        if ((count == 0) != (r.GetState() == RS_EMPTY))
            fail(r.GetId(), "state does not match player count", count);
        if (count == 0 && r.GetHostSlot() != -1)
            fail(r.GetId(), "empty room still has a host slot", r.GetHostSlot());
        if (count > 0 && (r.GetHostSlot() < 0 || r.GetHostSlot() >= MAX_ROOM_PLAYER
                          || r.GetMember(r.GetHostSlot()) == -1))
            fail(r.GetId(), "host slot is empty", r.GetHostSlot());

        for (int slot = 0; slot < MAX_ROOM_PLAYER; ++slot) {
            const int id = r.GetMember(slot);
            if (id == -1) continue;
            if (id < 0 || id >= MAX_USER) { fail(r.GetId(), "bad session id in a slot", id); continue; }
            if (inRoom[id]) fail(r.GetId(), "session is in two places", id);
            inRoom[id] = true;

            const SESSION& s = g_sessions[id];
            if (s.GetState() != ST_ROOM && s.GetState() != ST_INGAME)
                fail(r.GetId(), "member session is not in a room state (left or disconnected?)", id);
            else if (s.GetRoomId() != r.GetId() || s.GetSlot() != slot)
                fail(r.GetId(), "member session points to another room/slot", id);
            else if ((s.GetState() == ST_INGAME) != (r.GetState() == RS_PLAYING))
                fail(r.GetId(), "member game state does not match room state", id);
        }
    }

    for (int id = 0; id < MAX_USER; ++id) {
        const SESSION& s = g_sessions[id];
        const bool roomState = s.GetState() == ST_ROOM || s.GetState() == ST_INGAME;
        if (roomState && !inRoom[id])
            fail(s.GetRoomId(), "session thinks it is in a room but no room has it", id);
        if (!roomState && s.GetRoomId() != -1)
            fail(s.GetRoomId(), "session outside rooms still has a room id", id);
    }
    return ok;
}

void RoomManager::BroadcastRoomListToLobby(int exceptId)
{
    S2C_RoomList list;
    FillRoomList(list);
    for (SESSION& s : g_sessions)
        if (s.GetState() == ST_LOBBY && s.GetId() != exceptId)
            s.SendPacket(&list);
}

void RoomManager::FillRoomList(S2C_RoomList& out) const
{
    out = {};
    out.size = sizeof(out);
    out.type = S2C_ROOM_LIST;

    for (int i = 0; i < MAX_ROOM; ++i) {
        const Room&   r = m_rooms[i];
        ROOM_SUMMARY& s = out.rooms[i];

        s.roomId      = static_cast<unsigned char>(r.GetId());
        s.mapId       = static_cast<MAP_ID>(r.GetMapId());
        s.playerCount = static_cast<unsigned char>(r.GetPlayerCount());
        s.state       = r.GetState() == RS_EMPTY   ? ROOM_EMPTY
                      : r.GetState() == RS_WAITING ? ROOM_WAITING
                                                   : ROOM_PLAYING;

        if (r.GetHostSlot() != -1) {
            int hostId = r.GetMember(r.GetHostSlot());
            memcpy(s.hostName, g_sessions[hostId].GetName(), MAX_NAME_LEN);
        }
    }
}
