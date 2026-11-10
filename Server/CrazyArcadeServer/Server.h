#pragma once
#include <WinSock2.h>
#include "protocol.h"

// 게임 서버 본체: 리슨 소켓을 만들고 논블로킹 메인 루프를 돌린다.
class Server {
public:
    // WSAStartup → socket → bind → listen → ioctlsocket(FIONBIO)
    // bindIp가 nullptr이면 모든 네트워크 카드(INADDR_ANY)에서 받는다.
    bool Init(unsigned short port, const char* bindIp = nullptr);

    // 메인 루프 (Ctrl+C 또는 창 닫기로 종료)
    void Run();

    void Shutdown();

private:
    SOCKET m_listen = INVALID_SOCKET;
    bool   m_wsaStarted = false;
};

// 소켓 에러를 "[where] 코드: 설명" 형식으로 출력
void PrintSocketError(const char* where, int err);
