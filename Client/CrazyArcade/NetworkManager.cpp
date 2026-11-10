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
std::wstring NetworkManager::sLastError;
bool NetworkManager::sConnectionLost = false;

namespace {
    // 없는 IP로 블로킹 connect를 하면 20초 넘게 창이 멈추므로 최대 이만큼만 기다린다
    constexpr int kConnectTimeoutMs = 4000;

    std::wstring ConnectErrorText(int err)
    {
        switch (err) {
        case WSAECONNREFUSED: return L"서버가 꺼져 있거나 포트가 다릅니다";
        case WSAETIMEDOUT:    return L"서버 응답이 없습니다 (IP 주소를 확인하세요)";
        case WSAENETUNREACH:
        case WSAEHOSTUNREACH: return L"네트워크에 연결할 수 없습니다";
        default:              return L"서버에 접속할 수 없습니다 (오류 " + std::to_wstring(err) + L")";
        }
    }

    // S2C 패킷은 모두 고정 크기 struct이므로 type별 크기가 정확히 맞아야 한다.
    // 검사하지 않으면 헤더만 온 패킷을 struct로 읽을 때 버퍼 밖을 읽고 쓰게 된다. 0 = 모르는 type
    int ExpectedSize(unsigned char type)
    {
        switch (type) {
        case S2C_ECHO:              return sizeof(S2C_Echo);
        case S2C_LOGIN_RESULT:      return sizeof(S2C_LoginResult);
        case S2C_ROOM_LIST:         return sizeof(S2C_RoomList);
        case S2C_ROOM_ENTER_RESULT: return sizeof(S2C_RoomEnterResult);
        case S2C_ROOM_INFO:         return sizeof(S2C_RoomInfo);
        case S2C_PLAYER_LEAVE:      return sizeof(S2C_PlayerLeave);
        case S2C_GAME_START:        return sizeof(S2C_GameStart);
        case S2C_MAP_INFO:          return sizeof(S2C_MapInfo);
        case S2C_MOVE:              return sizeof(S2C_Move);
        case S2C_ATTACK:            return sizeof(S2C_Attack);
        case S2C_EXPLODE:           return sizeof(S2C_Explode);
        case S2C_BLOCK_BREAK:       return sizeof(S2C_BlockBreak);
        case S2C_ITEM_SPAWN:        return sizeof(S2C_ItemSpawn);
        case S2C_ITEM_GET:          return sizeof(S2C_ItemGet);
        case S2C_HIT:               return sizeof(S2C_Hit);
        case S2C_STAGE_STATE:       return sizeof(S2C_StageState);
        case S2C_TRAIN:             return sizeof(S2C_Train);
        case S2C_GAME_OVER:         return sizeof(S2C_GameOver);
        default:                    return 0;
        }
    }
}

bool NetworkManager::Connect(const char* ip, unsigned short port)
{
    Disconnect();
    sLastError.clear();
    sConnectionLost = false;
    sLastEcho.clear();   // 다시 접속하면 에코 기록도 새로 센다
    sEchoCount = 0;

    if (!sWsaStarted) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            sLastError = L"윈속 초기화에 실패했습니다";
            return false;
        }
        sWsaStarted = true;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        sLastError = L"서버 IP 주소 형식이 잘못되었습니다";
        return false;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        sLastError = ConnectErrorText(WSAGetLastError());
        return false;
    }

    // 논블로킹으로 바꾼 뒤 connect를 걸고, select로 접속 완료를 최대 kConnectTimeoutMs까지만 기다린다.
    // 이후 recv/send도 논블로킹이라 게임 프레임이 네트워크를 기다리느라 멈추지 않는다
    u_long noblock = 1;
    ioctlsocket(sock, FIONBIO, &noblock);

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            closesocket(sock);
            sLastError = ConnectErrorText(err);
            return false;
        }

        fd_set writeSet, errorSet;
        FD_ZERO(&writeSet);
        FD_ZERO(&errorSet);
        FD_SET(sock, &writeSet);
        FD_SET(sock, &errorSet);
        timeval timeout = { kConnectTimeoutMs / 1000, (kConnectTimeoutMs % 1000) * 1000 };

        int ret = select(0, nullptr, &writeSet, &errorSet, &timeout);
        if (ret == 0) {
            closesocket(sock);
            sLastError = ConnectErrorText(WSAETIMEDOUT);
            return false;
        }
        if (ret == SOCKET_ERROR || FD_ISSET(sock, &errorSet)) {
            int soError = 0;
            int len = sizeof(soError);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soError), &len);
            closesocket(sock);
            sLastError = ConnectErrorText(soError ? soError : WSAGetLastError());
            return false;
        }
    }

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

    // 실패한 쪽(DoRecv/FlushSend)이 sLastError에 이유를 적어 둔다
    if (!DoRecv() || !FlushSend()) {
        Disconnect();
        sConnectionLost = true;
        OutputDebugStringW((L"[net] 연결 끊김: " + sLastError + L"\n").c_str());
    }
}

bool NetworkManager::ConsumeConnectionLost()
{
    bool lost = sConnectionLost;
    sConnectionLost = false;
    return lost;
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

        if (ret == SOCKET_ERROR) {   // WOULDBLOCK이면 지금 받을 게 없을 뿐 (정상)
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) return true;
            sLastError = (err == WSAECONNRESET)
                ? L"서버와의 연결이 끊어졌습니다"
                : L"서버에서 받는 중 오류가 났습니다 (오류 " + std::to_wstring(err) + L")";
            return false;
        }
        if (ret == 0) {              // 서버가 연결 종료
            sLastError = L"서버가 연결을 종료했습니다";
            return false;
        }

        unsigned char* packetStart = sRecvBuf;
        int remain = sPrevRemain + ret;

        // 헤더(3바이트)조차 잘려서 올 수 있으므로 헤더가 다 왔는지부터 확인
        while (remain >= static_cast<int>(sizeof(PACKET_HEADER))) {
            int packetSize = reinterpret_cast<PACKET_HEADER*>(packetStart)->size;

            // 말이 안 되는 size → 잘못된 패킷, 접속 종료
            if (packetSize < static_cast<int>(sizeof(PACKET_HEADER)) || packetSize > MAX_PACKET_SIZE) {
                sLastError = L"서버에서 잘못된 패킷이 왔습니다 (size=" + std::to_wstring(packetSize) + L")";
                return false;
            }
            // 아직 덜 옴 → 다음 recv에서 이어 붙임
            if (packetSize > remain)
                break;

            if (!ProcessPacket(packetStart)) return false;   // 크기가 맞지 않는 패킷
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
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {   // 진짜 에러 → 접속 종료
                sLastError = L"서버로 보내는 중 오류가 났습니다 (오류 " + std::to_wstring(err) + L")";
                return false;
            }
            break;              // 지금은 못 보냄 → 다음 프레임에 이어서
        }
        // 실제로 보낸 만큼만 큐에서 지운다
        sSendQueue.erase(sSendQueue.begin(), sSendQueue.begin() + ret);
    }
    // 서버가 오랫동안 받지 못해 큐가 64KB를 넘으면 끊긴 것으로 본다
    if (sSendQueue.size() > MAX_SEND_QUEUE) {
        sLastError = L"서버가 응답하지 않습니다";
        return false;
    }
    return true;
}

void NetworkManager::SendEcho(const char* message)
{
    C2S_Echo p = {};
    p.size = sizeof(p);
    p.type = C2S_ECHO;
    strncpy_s(p.message, message, _TRUNCATE);
    SendPacket(&p);
}

bool NetworkManager::ProcessPacket(unsigned char* packet)
{
    auto header = reinterpret_cast<PACKET_HEADER*>(packet);

    int expected = ExpectedSize(header->type);
    if (expected == 0) {   // 모르는 type: 버전이 다른 서버일 수 있으니 끊지 않고 무시
        char msg[64];
        sprintf_s(msg, "[net] 모르는 패킷 type=%d 무시\n", header->type);
        OutputDebugStringA(msg);
        return true;
    }
    if (header->size != expected) {
        sLastError = L"서버 패킷 크기가 맞지 않습니다 (type=" + std::to_wstring(header->type) +
            L", size=" + std::to_wstring(header->size) + L")";
        return false;
    }

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
        sprintf_s(msg, "[net] 아직 처리하지 않는 패킷 type=%d\n", header->type);
        OutputDebugStringA(msg);
        break;
    }
    }
    return true;
}
