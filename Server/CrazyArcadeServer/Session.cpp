#include "Session.h"

void SESSION::Init(int id, SOCKET sock)
{
    m_id = id;
    m_socket = sock;
    m_state = ST_CONNECTED;
}

void SESSION::Close()
{
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    m_state = ST_FREE;
}
