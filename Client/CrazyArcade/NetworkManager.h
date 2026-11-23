#pragma once
#include "Common.h"
#include "protocol.h"

// 서버 연결과 패킷 송수신
class NetworkManager {
public:
    // 실패하면 이유는 LastError()
    static bool Connect(const char* ip, unsigned short port);
    static void Disconnect();
    static bool IsConnected() { return sSocket != INVALID_SOCKET; }
    // 종료할 때
    static void Shutdown();

    // 매 프레임 받고 보내기
    static void Poll();

    // 송신 큐에 넣기
    static void SendPacket(const void* packet);

    // 마지막 실패 이유
    static const std::wstring& LastError() { return sLastError; }
    // 끊겼으면 한 번만 true
    static bool ConsumeConnectionLost();

    // 보내기
    static void SendEcho(const char* message);
    // 넘치는 이름은 잘린다
    static void SendLogin(const std::string& nameUtf8);
    // 방 목록 요청
    static void SendRoomList();
    // 방 입장, 나가기
    static void SendJoinRoom(int roomId);
    static void SendLeaveRoom();
    // 대기방
    static void SendSelectMap(MAP_ID map);
    static void SendSelectColor(int color);
    static void SendReady(bool ready);
    static void SendStartGame();
    // 방향키를 누르거나 뗄 때만
    static void SendInput(INPUT_DIR dir);

private:
    static bool DoRecv();
    static bool FlushSend();
    // false면 접속 종료
    static bool ProcessPacket(unsigned char* packet);

    static SOCKET sSocket;
    static unsigned char sRecvBuf[RECV_BUF_SIZE];
    static int sPrevRemain;
    static std::vector<unsigned char> sSendQueue;
    static bool sWsaStarted;
    static std::wstring sLastError;
    static bool sConnectionLost;
};
