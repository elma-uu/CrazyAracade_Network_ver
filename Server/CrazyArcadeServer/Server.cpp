#include "Server.h"
#include "Session.h"
#include <WS2tcpip.h>
#include <atomic>
#include <iostream>
#include <string>
#include <vector>

// 1단계(에코 서버) 임시 세션 목록: 접속 순서대로 추가, 끊긴 자리는 재사용하지 않음
// → 11/7에 SESSION 배열 + FindFreeSession으로 바꿀 예정
static std::vector<SESSION> g_sessions;

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

        // 2. 받기 → 패킷 재조립 → ProcessPacket
        //    접속이 끊겼거나 잘못된 패킷이면 닫는다 (11/8에 Server::Disconnect로 정리)
        for (SESSION& s : g_sessions) {
            if (s.InUse() && !s.DoRecv()) {
                std::cout << "[" << s.GetId() << "] disconnected" << std::endl;
                s.Close();
            }
        }
        // 3. 게임 중인 방 갱신          (3단계)

        // 4. 쌓아 둔 패킷 보내기
        for (SESSION& s : g_sessions) {
            if (s.InUse() && !s.FlushSend()) {
                std::cout << "[" << s.GetId() << "] disconnected" << std::endl;
                s.Close();
            }
        }

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

        int id = static_cast<int>(g_sessions.size());
        g_sessions.emplace_back();
        g_sessions.back().Init(id, sock);

        char ip[INET_ADDRSTRLEN] = "?";
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        std::cout << "[" << id << "] connected from " << ip << ":"
                  << ntohs(addr.sin_port) << std::endl;
    }
}

void Server::Shutdown()
{
    for (SESSION& s : g_sessions)
        if (s.InUse())
            s.Close();

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
