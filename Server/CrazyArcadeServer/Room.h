#pragma once
#include <array>
#include <functional>
#include "protocol.h"
#include "GameWorld.h"   // Server/GameWorld.h (민규동)
#include "PlayerMovement.h"
#include "StageState.h"

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

    // 방장만 바꿀 수 있고, 바뀌면 전원 레디 해제
    bool SelectMap(int sessionId, int mapId);

    // 색은 0~6, 남이 쓰는 색은 안 됨
    bool SetColor(int slot, int color);

    // 레디 켜기/끄기만 한다. 게임을 시작하지는 않는다 (C2S_Ready 처리에서 호출 → SendRoomInfo)
    void SetReady(int slot, bool ready);
    bool IsReady(int slot) const { return m_ready[slot]; }

    // 2명 이상 && 방장 포함 전원 레디 (혼자서는 불가)
    bool CanStart() const;

    // 시작 칸은 맵 네 모서리
    static void GetStartTile(int slot, int& x, int& y);

    // 방 전원에게 S2C_GameStart를 보낸다
    void SendGameStart();

    // 게임 중 12ms마다 부른다
    void StepMovement(const std::function<bool(int, int)>& canWalk);
    PlayerMovement& Movement() { return m_move; }

    // 사과나무가 터지면 Stage().AddApples를 부르면 된다
    StageState& Stage() { return m_stage; }
    // 방 전원에게 S2C_StageState
    void SendStageState();

    // 대기 중 && 방장 && CanStart() 일 때만 StartGame() 후 true.
    // 안 되면 이유를 로그로 남기고 false
    bool RequestStart(int sessionId);

    // 게임 시작: GameWorld 판 준비 → RS_PLAYING → S2C_GameStart(전원 ST_INGAME) → S2C_MapInfo
    // 판 준비에 실패하면(맵 번호 오류) 시작하지 않고 false
    bool StartGame();

    // 게임 끝 (계획서 3.5): 전적 기록(11/30) → S2C_GameOver → 레디 초기화, RS_WAITING, 참가자 ST_ROOM
    // winnerSlot: 이긴 사람의 자리 0~3, 무승부면 -2 (S2C_GameOver의 winnerId는 -1)
    // reason: OVER_NORMAL(생존자 1명 이하) / OVER_OTHERS_LEFT(다른 사람이 모두 나감)
    // 게임 중이 아니면 아무 일도 하지 않는다
    void EndGame(int winnerSlot, unsigned char reason);

    // OnInput·OnAttack 등 인게임 처리에서 사용
    GameWorld& World() { return m_world; }

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

    // 이 방에서 진행 중인 게임 (판·물풍선·고정 스텝)
    GameWorld      m_world;
    PlayerMovement m_move;   // 게임 중 위치
    StageState     m_stage;  // 스테이지 상태
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

    // 방 인원·상태·맵·방장이 바뀌면 부른다 (입장·퇴장·접속 종료·맵 변경·게임 시작. 색·레디는 목록에 없음)
    // 바로 보내지 않고 "목록이 바뀜"으로 표시만 해 두고, 메인 루프가 보내기 직전에 FlushRoomList()로
    // 로비(ST_LOBBY) 전원에게 S2C_RoomList를 한 번만 보낸다.
    // (11/21: 한 루프 안에서 맵을 수천 번 바꾸는 식의 연타가 와도 로비에는 목록이 한 번만 간다)
    void BroadcastRoomListToLobby();

    // 표시가 있으면 로비 전원에게 지금 방 목록을 보내고 표시를 지운다. Server::Run이 루프마다 호출
    void FlushRoomList();

    // 방 기록(자리의 세션 ID)과 세션 기록(방 번호·자리·상태)이 서로 맞는지 검사.
    // 어긋난 곳마다 "[check] ..." 로그를 남기고 false. Debug 빌드에서 패킷 처리·접속 종료 직후마다
    // 불러서, 나간 사람이 방에 남는 것 같은 정리 누락을 생긴 순간에 찾는다.
    bool CheckConsistency() const;

    Room&       Get(int roomId)       { return m_rooms[roomId - 1]; }   // roomId 1~8
    const Room& Get(int roomId) const { return m_rooms[roomId - 1]; }

private:
    std::array<Room, MAX_ROOM> m_rooms;
    bool m_roomListDirty = false;   // 이번 루프에 로비 목록이 바뀌었는지
};

extern RoomManager g_roomManager;
