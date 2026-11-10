#include "NetworkManager.h"
#pragma comment(lib, "ws2_32.lib")

SOCKET NetworkManager::sSocket = INVALID_SOCKET;
unsigned char NetworkManager::sRecvBuf[RECV_BUF_SIZE] = {};
int NetworkManager::sPrevRemain = 0;
std::vector<unsigned char> NetworkManager::sSendQueue;
bool NetworkManager::sWsaStarted = false;

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
    // TODO(11/3): DoRecv → FlushSend
}

void NetworkManager::SendPacket(const void* packet)
{
    // TODO(11/2): 송신 큐
}

bool NetworkManager::DoRecv()
{
    // TODO(11/3): 패킷 재조립
    return true;
}

bool NetworkManager::FlushSend()
{
    // TODO(11/2)
    return true;
}

void NetworkManager::ProcessPacket(unsigned char* packet)
{
    // TODO(11/4~): 패킷 종류별 처리
}
