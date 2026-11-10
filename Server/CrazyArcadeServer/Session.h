#pragma once
#include <WinSock2.h>
#include "protocol.h"

enum SESSION_STATE {
    ST_FREE, ST_CONNECTED, ST_LOBBY, ST_ROOM, ST_INGAME
};

// 접속한 클라이언트 한 명
class SESSION {
public:
    // accept 직후: ST_CONNECTED, 버퍼 초기화
    void Init(int id, SOCKET sock);

    // closesocket 후 ST_FREE
    void Close();

    // 논블로킹 recv + 패킷 재조립 (false = 접속 종료)
    bool DoRecv();

    // 패킷 하나 처리 (재조립이 끝난 완성 패킷만 들어온다)
    void ProcessPacket(unsigned char* packet);

    bool InUse() const { return m_state != ST_FREE; }
    int  GetId() const { return m_id; }

private:
    int           m_id = -1;
    SOCKET        m_socket = INVALID_SOCKET;
    SESSION_STATE m_state = ST_FREE;

    // 받다 만 패킷 조각을 보관하는 버퍼
    unsigned char m_recvBuf[RECV_BUF_SIZE]{};
    int           m_prevRemain = 0;
};
