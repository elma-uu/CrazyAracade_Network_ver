#include "Server.h"
#include "Room.h"
#include "Session.h"
#include <WS2tcpip.h>
#include <atomic>
#include <iostream>
#include <string>

// 접속자 전체: 배열 번호 = 세션 ID. 끊긴 자리(ST_FREE)는 다음 접속자가 재사용한다.
std::array<SESSION, MAX_USER> g_sessions;

namespace {
    // Ctrl+C / 창 닫기에서 false로 바꿔 메인 루프를 끝낸다 (콘솔 핸들러는 다른 스레드에서 불림)
    std::atomic<bool> g_running{ true };

    BOOL WINAPI ConsoleHandler(DWORD)
    {
        g_running = false;
        return TRUE;
    }
}

void PrintSocketError(const char* where, int err)
{
    wchar_t* wmsg = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&wmsg), 0, nullptr);

    // 콘솔을 UTF-8로 쓰므로 시스템 메시지도 UTF-8로 바꿔서 출력
    std::string msg;
    if (wmsg) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wmsg, -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            msg.resize(len - 1);
            WideCharToMultiByte(CP_UTF8, 0, wmsg, -1, msg.data(), len, nullptr, nullptr);
        }
        LocalFree(wmsg);
    }
    while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r'))
        msg.pop_back();

    std::cout << "[error] " << where << " " << err << ": " << msg << std::endl;
}

bool Server::Init(unsigned short port, const char* bindIp)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cout << "[error] WSAStartup failed" << std::endl;
        return false;
    }
    m_wsaStarted = true;

    m_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listen == INVALID_SOCKET) {
        PrintSocketError("socket", WSAGetLastError());
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (bindIp == nullptr) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    else if (inet_pton(AF_INET, bindIp, &addr.sin_addr) != 1) {
        std::cout << "[error] invalid bind ip: " << bindIp << std::endl;
        return false;
    }

    if (bind(m_listen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        PrintSocketError("bind", WSAGetLastError());
        return false;
    }
    if (listen(m_listen, SOMAXCONN) == SOCKET_ERROR) {
        PrintSocketError("listen", WSAGetLastError());
        return false;
    }

    // 리슨 소켓을 논블로킹으로: 접속이 없어도 accept가 바로 돌아온다
    u_long noblock = 1;
    if (ioctlsocket(m_listen, FIONBIO, &noblock) == SOCKET_ERROR) {
        PrintSocketError("ioctlsocket", WSAGetLastError());
        return false;
    }

    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    std::cout << "[server] listening on " << (bindIp ? bindIp : "0.0.0.0")
              << ":" << port << std::endl;
    return true;
}

void Server::Run()
{
    std::cout << "[server] running (Ctrl+C to stop)" << std::endl;

    while (g_running) {
        // 1. 새 접속 받기 (없으면 바로 넘어감)
        AcceptClients();

        // 2. 받기 → 패킷 재조립 → ProcessPacket (접속 종료·잘못된 패킷이면 끊기)
        for (SESSION& s : g_sessions)
            if (s.InUse() && !s.DoRecv())
                Disconnect(s.GetId());

        // 3. 게임 중인 방 갱신          (3단계)

        // 이번 루프에 방 목록이 바뀌었으면 로비 전원에게 한 번만 S2C_RoomList (11/21)
        g_roomManager.FlushRoomList();

        // 4. 쌓아 둔 패킷 보내기 (send 에러·송신 큐 초과면 끊기)
        for (SESSION& s : g_sessions)
            if (s.InUse() && !s.FlushSend())
                Disconnect(s.GetId());

        // 5. CPU 사용량 줄이기
        Sleep(1);
    }
}

void Server::AcceptClients()
{
    while (true) {
        sockaddr_in addr{};
        int addrLen = sizeof(addr);
        SOCKET sock = accept(m_listen, reinterpret_cast<sockaddr*>(&addr), &addrLen);

        if (sock == INVALID_SOCKET) {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK)          // WOULDBLOCK = 지금은 새 접속 없음 (정상)
                PrintSocketError("accept", err);
            return;
        }

        // 클라 소켓도 논블로킹 + Nagle 끄기 (작은 패킷을 모으지 않고 바로 보냄)
        u_long noblock = 1;
        ioctlsocket(sock, FIONBIO, &noblock);
        BOOL noDelay = TRUE;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
                   reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

        char ip[INET_ADDRSTRLEN] = "?";
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));

        int id = FindFreeSession();
        if (id == -1) {
            // 이미 MAX_USER명 접속 중 → 받자마자 끊는다
            std::cout << "[server] full (" << MAX_USER << " users), refused "
                      << ip << ":" << ntohs(addr.sin_port) << std::endl;
            closesocket(sock);
            continue;
        }

        g_sessions[id].Init(id, sock);
        std::cout << "[" << id << "] connected from " << ip << ":"
                  << ntohs(addr.sin_port) << std::endl;
    }
}

int Server::FindFreeSession() const
{
    for (int i = 0; i < MAX_USER; ++i)
        if (!g_sessions[i].InUse())
            return i;
    return -1;
}

// 계획서 3.4의 정리 순서. 2단계 이후 기능이 생기면 아래 자리에 채워 넣는다.
void Server::Disconnect(int id)
{
    if (id < 0 || id >= MAX_USER)
        return;
    SESSION& s = g_sessions[id];
    if (!s.InUse())          // 이미 정리된 자리 (두 번 불려도 안전)
        return;

    // 1) 게임 중이었으면 탈락 처리, 생존자 1명이면 S2C_GameOver   (11/26)
    // 2) 방에 있었으면 퇴장 처리, 방장이었으면 위임 → 남은 사람에게 S2C_PlayerLeave·S2C_RoomInfo
    //    (빼지 않으면 같은 세션 번호로 들어온 다음 접속자가 그 방에 있는 것으로 보인다)
    //    → 로비 전원에게 바뀐 방 목록
    if (g_roomManager.FindRoomOf(id) != -1) {
        g_roomManager.LeaveRoom(id);
        g_roomManager.BroadcastRoomListToLobby();   // 끊긴 세션은 보낼 때 이미 ST_FREE라 받지 않는다
    }
    // 3) 전적 파일 저장                                           (11/30)
    // 4) closesocket 후 ST_FREE로 되돌려 다음 접속자가 쓰게 한다
    s.Reset();
    std::cout << "[" << id << "] disconnected" << std::endl;

#ifdef _DEBUG
    g_roomManager.CheckConsistency();   // 나간 사람이 어느 방에도 남지 않았는지 (Debug 빌드만)
#endif
}

void Server::Shutdown()
{
    for (int i = 0; i < MAX_USER; ++i)
        Disconnect(i);

    if (m_listen != INVALID_SOCKET) {
        closesocket(m_listen);
        m_listen = INVALID_SOCKET;
    }
    if (m_wsaStarted) {
        WSACleanup();
        m_wsaStarted = false;
    }
    std::cout << "[server] shutdown" << std::endl;
}
