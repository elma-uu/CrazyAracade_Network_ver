#pragma once
#include <WinSock2.h>
#include "protocol.h"

// 게임 서버 본체: 리슨 소켓을 만들고 논블로킹 메인 루프를 돌린다.
class Server {
public:
    // WSAStartup → socket → bind → listen → ioctlsocket(FIONBIO)
    bool Init(unsigned short port);

    // 메인 루프
    void Run();

    void Shutdown();

private:
    SOCKET m_listen = INVALID_SOCKET;
};
