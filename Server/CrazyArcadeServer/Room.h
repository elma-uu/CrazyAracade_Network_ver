#pragma once
#include <array>
#include "protocol.h"

enum ROOM_STATE { RS_EMPTY, RS_WAITING, RS_PLAYING };

// 방 하나 (1~8번 고정). 방장·맵·네 자리(세션 ID·색상·레디)를 관리한다.
class Room {
public:
    void Init(int id);

    // 정원·게임 중 검사, 빈 방이면 입장한 사람이 방장
    // 성공: outSlot = 앉은 자리(0~3) / 실패: failReason = ENTER_FULL 또는 ENTER_PLAYING
    bool Enter(int sessionId, int& outSlot, unsigned char& failReason);

    // 방장이 나가면 남은 사람 중 가장 먼저 들어온 사람에게 위임
    // → 남은 사람 전원에게 S2C_PlayerLeave, S2C_RoomInfo (나간 사람에게는 보내지 않음)
    // 마지막 사람이 나가면 빈 방(RS_EMPTY)으로 되돌린다
    void Leave(int sessionId);

    bool IsHost(int sessionId) const;
    int  FindSlot(int sessionId) const;   // 이 방에 없으면 -1
    int  GetPlayerCount() const;

    // 방 안 전원에게 같은 패킷 (exceptId 세션은 빼고)
    void Broadcast(const void* packet, int exceptId = -1);

    // 방 안 전원에게 S2C_RoomInfo. 입장·퇴장·맵·색상·레디처럼 방 안에서 무언가 바뀔 때마다 부른다
    void SendRoomInfo();
    void FillRoomInfo(S2C_RoomInfo& out) const;

    // 방장만 (1 숲, 2 도시, 3 얼음), 바꾸면 전원 레디 해제.
    // 대기 중이 아니거나 방장이 아니거나 맵 번호가 잘못되면 false (로그를 남김)
    // 같은 맵을 다시 고르면 바뀐 것이 없으므로 true만 돌려주고 레디는 그대로 둔다
    bool SelectMap(int sessionId, int mapId);

    // 레디 켜기/끄기만 한다. 게임을 시작하지는 않는다 (C2S_Ready 처리에서 호출 → SendRoomInfo)
    void SetReady(int slot, bool ready);
    bool IsReady(int slot) const { return m_ready[slot]; }

    // 2명 이상 && 방장 포함 전원 레디 (혼자서는 불가)
    bool CanStart() const;

    // 대기 중 && 방장 && CanStart() 일 때만 true. 안 되면 이유를 로그로 남기고 false
    // (조건을 통과하면 11/17부터 StartGame()으로 게임을 시작한다)
    bool RequestStart(int sessionId);

    int        GetId() const              { return m_id; }
    ROOM_STATE GetState() const           { return m_state; }
    int        GetMapId() const           { return m_mapId; }
    int        GetHostSlot() const        { return m_hostSlot; }   // 빈 방이면 -1
    int        GetMember(int slot) const  { return m_members[slot]; }

private:
    int        m_id = 0;
    ROOM_STATE m_state = RS_EMPTY;
    int        m_mapId = MAP_FOREST;
    int        m_hostSlot = -1;

    int           m_members[MAX_ROOM_PLAYER] = { -1, -1, -1, -1 };  // 세션 ID, -1 = 빈 자리
    unsigned char m_color[MAX_ROOM_PLAYER]{};
    bool          m_ready[MAX_ROOM_PLAYER]{};

    // 들어온 순서 (방장 위임 기준). 자리 번호는 재사용되므로 순서를 따로 센다.
    unsigned int m_enterOrder[MAX_ROOM_PLAYER]{};
    unsigned int m_nextOrder = 0;
};

// 방 1~8번 고정, 항상 로비에 표시
class RoomManager {
public:
    RoomManager();

    static bool IsValidRoomId(int roomId) { return 1 <= roomId && roomId <= MAX_ROOM; }

    // 방 번호가 잘못됐거나 이미 다른 방에 있으면 false (failReason은 그대로 ENTER_OK)
    bool JoinRoom(int sessionId, int roomId, int& outSlot, unsigned char& failReason);

    // 들어가 있는 방에서 나간다 (방에 없으면 아무 일도 하지 않음)
    void LeaveRoom(int sessionId);

    // 들어가 있는 방 번호, 없으면 -1
    int  FindRoomOf(int sessionId) const;

    // 로비 방 목록 (S2C_RoomList)에 담을 방 1~8의 맵·인원·상태·방장 이름
    void FillRoomList(S2C_RoomList& out) const;

    Room&       Get(int roomId)       { return m_rooms[roomId - 1]; }   // roomId 1~8
    const Room& Get(int roomId) const { return m_rooms[roomId - 1]; }

private:
    std::array<Room, MAX_ROOM> m_rooms;
};

extern RoomManager g_roomManager;
