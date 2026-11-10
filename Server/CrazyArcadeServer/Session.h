#pragma once
#include <WinSock2.h>
#include "protocol.h"

enum SESSION_STATE {
    ST_FREE, ST_CONNECTED, ST_LOBBY, ST_ROOM, ST_INGAME
};

// 접속한 클라이언트 한 명
class SESSION {
public:
    // accept 직후: ST_CONNECTED
    void Init(int id, SOCKET sock);

    // closesocket 후 ST_FREE
    void Close();

    bool InUse() const { return m_state != ST_FREE; }
    int  GetId() const { return m_id; }

private:
    int           m_id = -1;
    SOCKET        m_socket = INVALID_SOCKET;
    SESSION_STATE m_state = ST_FREE;
};
