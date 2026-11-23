#include "NetworkManager.h"
#include "GameClient.h"
#include <cstdio>
#include <cstring>
#pragma comment(lib, "ws2_32.lib")

SOCKET NetworkManager::sSocket = INVALID_SOCKET;
unsigned char NetworkManager::sRecvBuf[RECV_BUF_SIZE] = {};
int NetworkManager::sPrevRemain = 0;
std::vector<unsigned char> NetworkManager::sSendQueue;
bool NetworkManager::sWsaStarted = false;
std::wstring NetworkManager::sLastError;
bool NetworkManager::sConnectionLost = false;

namespace {
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

    // 종류별 패킷 크기, 0이면 모르는 종류
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

    // 블로킹으로 접속한다
    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        sLastError = ConnectErrorText(WSAGetLastError());
        closesocket(sock);
        return false;
    }

    // 이후로는 논블로킹
    u_long noblock = 1;
    ioctlsocket(sock, FIONBIO, &noblock);

    // Nagle 끄기
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

    // 실패 이유는 sLastError에 남는다
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
    // 큐에 쌓았다가 FlushSend에서 보낸다
    auto p = static_cast<const unsigned char*>(packet);
    int size = reinterpret_cast<const PACKET_HEADER*>(p)->size;
    sSendQueue.insert(sSendQueue.end(), p, p + size);
}

// 받은 데이터를 모아 패킷 단위로 자른다
bool NetworkManager::DoRecv()
{
    while (IsConnected()) {
        // 지난번에 남은 조각 뒤에 이어서 받는다
        char* buf = reinterpret_cast<char*>(sRecvBuf) + sPrevRemain;
        int space = RECV_BUF_SIZE - sPrevRemain;
        int ret = recv(sSocket, buf, space, 0);

        if (ret == SOCKET_ERROR) {   // WOULDBLOCK은 정상
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

        // 헤더가 다 왔는지부터 본다
        while (remain >= static_cast<int>(sizeof(PACKET_HEADER))) {
            int packetSize = reinterpret_cast<PACKET_HEADER*>(packetStart)->size;

            // size가 이상하면 끊는다
            if (packetSize < static_cast<int>(sizeof(PACKET_HEADER)) || packetSize > MAX_PACKET_SIZE) {
                sLastError = L"서버에서 잘못된 패킷이 왔습니다 (size=" + std::to_wstring(packetSize) + L")";
                return false;
            }
            // 아직 덜 왔음
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
            if (err != WSAEWOULDBLOCK) {   // 진짜 에러
                sLastError = L"서버로 보내는 중 오류가 났습니다 (오류 " + std::to_wstring(err) + L")";
                return false;
            }
            break;              // 다음 프레임에 다시
        }
        // 실제로 보낸 만큼만 큐에서 지운다
        sSendQueue.erase(sSendQueue.begin(), sSendQueue.begin() + ret);
    }
    // 64KB 넘게 쌓이면 끊긴 걸로 본다
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

void NetworkManager::SendLogin(const std::string& nameUtf8)
{
    C2S_Login p = {};
    p.size = sizeof(p);
    p.type = C2S_LOGIN;
    // 끝 '\0' 자리는 남긴다
    memcpy(p.name, nameUtf8.data(), std::min(nameUtf8.size(), sizeof(p.name) - 1));
    SendPacket(&p);
}

void NetworkManager::SendRoomList()
{
    C2S_RoomList p = {};
    p.size = sizeof(p);
    p.type = C2S_ROOM_LIST;
    SendPacket(&p);
}

void NetworkManager::SendJoinRoom(int roomId)
{
    C2S_JoinRoom p = {};
    p.size = sizeof(p);
    p.type = C2S_JOIN_ROOM;
    p.roomId = static_cast<unsigned char>(roomId);
    SendPacket(&p);
}

void NetworkManager::SendLeaveRoom()
{
    C2S_LeaveRoom p = {};
    p.size = sizeof(p);
    p.type = C2S_LEAVE_ROOM;
    SendPacket(&p);
}

void NetworkManager::SendSelectMap(MAP_ID map)
{
    C2S_SelectMap p = {};
    p.size = sizeof(p);
    p.type = C2S_SELECT_MAP;
    p.mapId = map;
    SendPacket(&p);
}

void NetworkManager::SendSelectColor(int color)
{
    C2S_SelectColor p = {};
    p.size = sizeof(p);
    p.type = C2S_SELECT_COLOR;
    p.color = static_cast<unsigned char>(color);
    SendPacket(&p);
}

void NetworkManager::SendReady(bool ready)
{
    C2S_Ready p = {};
    p.size = sizeof(p);
    p.type = C2S_READY;
    p.ready = ready;
    SendPacket(&p);
}

void NetworkManager::SendStartGame()
{
    C2S_StartGame p = {};
    p.size = sizeof(p);
    p.type = C2S_START_GAME;
    SendPacket(&p);
}

void NetworkManager::SendInput(INPUT_DIR dir)
{
    C2S_Input p = {};
    p.size = sizeof(p);
    p.type = C2S_INPUT;
    p.dir = dir;
    SendPacket(&p);
}

bool NetworkManager::ProcessPacket(unsigned char* packet)
{
    auto header = reinterpret_cast<PACKET_HEADER*>(packet);

    int expected = ExpectedSize(header->type);
    if (expected == 0) {   // 모르는 패킷은 무시
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
    case S2C_ECHO:
        GameClient::OnEcho(reinterpret_cast<S2C_Echo*>(packet));
        break;
    case S2C_LOGIN_RESULT:
        GameClient::OnLoginResult(reinterpret_cast<S2C_LoginResult*>(packet));
        break;
    case S2C_ROOM_LIST:
        GameClient::OnRoomList(reinterpret_cast<S2C_RoomList*>(packet));
        break;
    case S2C_ROOM_ENTER_RESULT:
        GameClient::OnRoomEnterResult(reinterpret_cast<S2C_RoomEnterResult*>(packet));
        break;
    case S2C_ROOM_INFO:
        GameClient::OnRoomInfo(reinterpret_cast<S2C_RoomInfo*>(packet));
        break;
    case S2C_GAME_START:
        GameClient::OnGameStart(reinterpret_cast<S2C_GameStart*>(packet));
        break;
    case S2C_MOVE:
        GameClient::OnMove(reinterpret_cast<S2C_Move*>(packet));
        break;
    case S2C_PLAYER_LEAVE:
        GameClient::OnPlayerLeave(reinterpret_cast<S2C_PlayerLeave*>(packet));
        break;
    default: {
        char msg[64];
        sprintf_s(msg, "[net] 아직 처리하지 않는 패킷 type=%d\n", header->type);
        OutputDebugStringA(msg);
        break;
    }
    }
    return true;
}
