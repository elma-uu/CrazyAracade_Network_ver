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

    // WSAEWOULDBLOCK이 나올 때까지 accept 반복
    // 새 소켓도 FIONBIO + TCP_NODELAY로 설정
    void AcceptClients();

    // ST_FREE인 SESSION 번호 (없으면 -1 → 접속 거부)
    int  FindFreeSession() const;

    // 방 퇴장·탈락 처리 → closesocket → SESSION::Reset
    // 이미 끊긴 세션에 다시 불려도 아무 일도 하지 않는다
    void Disconnect(int id);

    void Shutdown();

private:
    SOCKET m_listen = INVALID_SOCKET;
    bool   m_wsaStarted = false;
};

// 소켓 에러를 "[where] 코드: 설명" 형식으로 출력
void PrintSocketError(const char* where, int err);
