#pragma once
#include <WinSock2.h>
#include <array>
#include <vector>
#include "protocol.h"

enum SESSION_STATE {
    ST_FREE, ST_CONNECTED, ST_LOBBY, ST_ROOM, ST_INGAME
};

// 접속한 클라이언트 한 명
class SESSION {
public:
    // accept 직후: ST_CONNECTED, 버퍼 초기화
    void Init(int id, SOCKET sock);

    // closesocket, 버퍼·큐 비움, ST_FREE (다른 클라이언트가 재사용)
    // 직접 부르지 말고 Server::Disconnect를 거칠 것
    void Reset();

    // 논블로킹 recv + 패킷 재조립 (false = 접속 종료)
    bool DoRecv();

    // 패킷 하나 처리 (재조립이 끝난 완성 패킷만 들어온다)
    // false = 크기가 맞지 않는 잘못된 패킷 → 접속 종료
    bool ProcessPacket(unsigned char* packet);

    // size만큼 송신 큐에 복사 (실제 전송은 메인 루프 끝의 FlushSend)
    void SendPacket(const void* packet);

    // 큐를 send, 보낸 만큼만 큐에서 제거 (false = 접속 종료)
    bool FlushSend();

    // 송신 헬퍼
    void SendLoginResult(bool success);
    void SendRoomEnterResult(bool success, int roomId, int slot, unsigned char failReason);
    void SendRoomList();   // 로비 진입·새로고침·방 상태 변경 시

    // S2C_GameStart를 받은 뒤: 방(ST_ROOM) → 게임 중(ST_INGAME). Room::SendGameStart에서 호출
    void EnterGame();

    bool InUse() const { return m_state != ST_FREE; }
    int  GetId() const { return m_id; }
    SESSION_STATE GetState() const { return m_state; }
    const char* GetName() const { return m_name; }   // 로그인 전이면 ""
    int  GetRoomId() const { return m_roomId; }       // -1 = 로비
    int  GetSlot() const { return m_slot; }           // 방 안 자리 0~3, 로비면 -1

    // 로그인을 마친 세션인지 (S2C_LoginResult 성공 후 ST_LOBBY 이상)
    bool IsLoggedIn() const { return m_state >= ST_LOBBY; }

private:
    // C2S 핸들러 (ProcessPacket에서 호출)
    void OnEcho(const C2S_Echo* p);
    void OnLogin(const C2S_Login* p);
    void OnRoomList(const C2S_RoomList* p);       // 로비 방 목록 요청
    void OnJoinRoom(const C2S_JoinRoom* p);       // 로비에서 방 선택
    void OnLeaveRoom(const C2S_LeaveRoom* p);     // 방 → 로비
    void OnSelectMap(const C2S_SelectMap* p);     // 방장만
    void OnSelectColor(const C2S_SelectColor* p);
    void OnReady(const C2S_Ready* p);             // 레디 토글
    void OnStartGame(const C2S_StartGame* p);     // 방장만

    int           m_id = -1;
    SOCKET        m_socket = INVALID_SOCKET;
    SESSION_STATE m_state = ST_FREE;
    char          m_name[MAX_NAME_LEN]{};   // UTF-8 닉네임, 로그인 전에는 ""
    int           m_roomId = -1;            // -1 = 로비
    int           m_slot = -1;              // 방 안 자리 0~3

    // 받다 만 패킷 조각을 보관하는 버퍼
    unsigned char m_recvBuf[RECV_BUF_SIZE]{};
    int           m_prevRemain = 0;

    // 아직 못 보낸 데이터 (논블로킹 send의 부분 전송 대비)
    std::vector<unsigned char> m_sendQueue;
};

// 접속자 전체 (배열 번호 = 세션 ID, 최대 MAX_USER명). 정의는 Server.cpp
extern std::array<SESSION, MAX_USER> g_sessions;
