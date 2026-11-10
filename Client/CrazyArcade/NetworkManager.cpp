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
    if (!IsConnected()) return;
    // 바로 send하지 않고 큐에 쌓아 둔다: 논블로킹 send는 일부만 보내거나 WOULDBLOCK을 줄 수 있다
    auto p = static_cast<const unsigned char*>(packet);
    int size = reinterpret_cast<const PACKET_HEADER*>(p)->size;
    sSendQueue.insert(sSendQueue.end(), p, p + size);
}

bool NetworkManager::DoRecv()
{
    // TODO(11/3): 패킷 재조립
    return true;
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

void NetworkManager::ProcessPacket(unsigned char* packet)
{
    // TODO(11/4~): 패킷 종류별 처리
}
