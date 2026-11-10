#include "NetworkManager.h"
#pragma comment(lib, "ws2_32.lib")

SOCKET NetworkManager::sSocket = INVALID_SOCKET;
unsigned char NetworkManager::sRecvBuf[RECV_BUF_SIZE] = {};
int NetworkManager::sPrevRemain = 0;
std::vector<unsigned char> NetworkManager::sSendQueue;

bool NetworkManager::Connect(const char* ip, unsigned short port)
{
    // TODO(11/1): WSAStartup → socket → connect → FIONBIO
    return false;
}

void NetworkManager::Disconnect()
{
    // TODO(11/1)
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
