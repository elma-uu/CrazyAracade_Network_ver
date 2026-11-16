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
