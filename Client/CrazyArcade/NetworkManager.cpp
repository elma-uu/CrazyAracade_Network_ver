#include "NetworkManager.h"
#include <cstdio>
#include <cstring>
#pragma comment(lib, "ws2_32.lib")

SOCKET NetworkManager::sSocket = INVALID_SOCKET;
unsigned char NetworkManager::sRecvBuf[RECV_BUF_SIZE] = {};
int NetworkManager::sPrevRemain = 0;
std::vector<unsigned char> NetworkManager::sSendQueue;
bool NetworkManager::sWsaStarted = false;
std::string NetworkManager::sLastEcho;
int NetworkManager::sEchoCount = 0;

bool NetworkManager::Connect(const char* ip, unsigned short port)
{
    Disconnect();

    if (!sWsaStarted) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
        sWsaStarted = true;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return false;

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1 ||
        connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return false;
    }

    // 접속이 끝난 뒤에는 논블로킹으로: recv/send가 기다리느라 게임 프레임이 멈추지 않게
    u_long noblock = 1;
    ioctlsocket(sock, FIONBIO, &noblock);
    // 키 입력 같은 작은 패킷을 모아 보내지 않도록 Nagle 끄기
    BOOL nodelay = TRUE;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));

    sSocket = sock;
    sPrevRemain = 0;
    sSendQueue.clear();
    return true;
}

void NetworkManager::Disconnect()
{
    if (sSocket != INVALID_SOCKET) {
        closesocket(sSocket);
        sSocket = INVALID_SOCKET;
    }
    sPrevRemain = 0;
    sSendQueue.clear();
}

void NetworkManager::Shutdown()
{
    Disconnect();
    if (sWsaStarted) {
        WSACleanup();
        sWsaStarted = false;
    }
}

void NetworkManager::Poll()
{
    if (!IsConnected()) return;

    if (!DoRecv() || !FlushSend())
        Disconnect();
}

void NetworkManager::SendPacket(const void* packet)
{
    if (!IsConnected()) return;
    // 바로 send하지 않고 큐에 쌓아 둔다: 논블로킹 send는 일부만 보내거나 WOULDBLOCK을 줄 수 있다
    auto p = static_cast<const unsigned char*>(packet);
    int size = reinterpret_cast<const PACKET_HEADER*>(p)->size;
    sSendQueue.insert(sSendQueue.end(), p, p + size);
}

// 서버 SESSION::DoRecv와 같은 재조립 코드.
// TCP는 보낸 단위 그대로 오지 않으므로(붙어서 오거나 잘려서 옴) 버퍼에 모아 두었다가
// 맨 앞 헤더의 size만큼 다 모였을 때만 패킷 하나로 처리한다.
bool NetworkManager::DoRecv()
{
    while (IsConnected()) {
        // 지난번에 남은 조각 뒤에 이어서 받는다
        char* buf = reinterpret_cast<char*>(sRecvBuf) + sPrevRemain;
        int space = RECV_BUF_SIZE - sPrevRemain;
        int ret = recv(sSocket, buf, space, 0);

        if (ret == SOCKET_ERROR)   // WOULDBLOCK이면 지금 받을 게 없을 뿐 (정상)
            return WSAGetLastError() == WSAEWOULDBLOCK;
        if (ret == 0)              // 서버가 연결 종료
            return false;

        unsigned char* packetStart = sRecvBuf;
        int remain = sPrevRemain + ret;

        // 헤더(3바이트)조차 잘려서 올 수 있으므로 헤더가 다 왔는지부터 확인
        while (remain >= static_cast<int>(sizeof(PACKET_HEADER))) {
            int packetSize = reinterpret_cast<PACKET_HEADER*>(packetStart)->size;

            // 말이 안 되는 size → 잘못된 패킷, 접속 종료
            if (packetSize < static_cast<int>(sizeof(PACKET_HEADER)) || packetSize > MAX_PACKET_SIZE)
                return false;
            // 아직 덜 옴 → 다음 recv에서 이어 붙임
            if (packetSize > remain)
                break;

            ProcessPacket(packetStart);
            if (!IsConnected()) return false;   // 처리 중에 연결을 끊은 경우
            packetStart += packetSize;
            remain -= packetSize;
        }

        // 남은 조각을 버퍼 앞으로 옮겨 둔다
        sPrevRemain = remain;
        if (remain > 0)
            memmove(sRecvBuf, packetStart, remain);
    }
    return false;
}

bool NetworkManager::FlushSend()
{
    while (!sSendQueue.empty()) {
        auto data = reinterpret_cast<const char*>(sSendQueue.data());
        int ret = send(sSocket, data, static_cast<int>(sSendQueue.size()), 0);

        if (ret == SOCKET_ERROR) {
            if (WSAGetLastError() != WSAEWOULDBLOCK)
                return false;   // 진짜 에러 → 접속 종료
            break;              // 지금은 못 보냄 → 다음 프레임에 이어서
        }
        // 실제로 보낸 만큼만 큐에서 지운다
        sSendQueue.erase(sSendQueue.begin(), sSendQueue.begin() + ret);
    }
    // 서버가 오랫동안 받지 못해 큐가 64KB를 넘으면 끊긴 것으로 본다
    return sSendQueue.size() <= MAX_SEND_QUEUE;
}

void NetworkManager::SendEcho(const char* message)
{
    C2S_Echo p = {};
    p.size = sizeof(p);
    p.type = C2S_ECHO;
    strncpy_s(p.message, message, _TRUNCATE);
    SendPacket(&p);
}

void NetworkManager::ProcessPacket(unsigned char* packet)
{
    auto header = reinterpret_cast<PACKET_HEADER*>(packet);
    switch (header->type) {
    case S2C_ECHO: {
        auto p = reinterpret_cast<S2C_Echo*>(packet);
        p->message[sizeof(p->message) - 1] = '\0';   // 서버가 '\0'을 빠뜨려도 안전하게
        sLastEcho = p->message;
        ++sEchoCount;
        OutputDebugStringA(("[net] S2C_Echo: " + sLastEcho + "\n").c_str());
        break;
    }
    default: {
        char msg[64];
        sprintf_s(msg, "[net] 처리하지 않는 패킷 type=%d\n", header->type);
        OutputDebugStringA(msg);
        break;
    }
    }
}
