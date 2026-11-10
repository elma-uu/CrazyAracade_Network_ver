#pragma once
#include "Common.h"
#include "protocol.h"

// 서버 연결과 패킷 송수신만 담당한다 (화면·게임 상태는 모른다).
// 소켓은 논블로킹이며, 매 프레임 Poll()에서 받기 → 재조립 → 처리 → 보내기를 한 번에 한다.
class NetworkManager {
public:
    // 논블로킹 connect + select로 최대 4초 대기. 실패하면 false, 이유는 LastError()
    static bool Connect(const char* ip, unsigned short port);
    static void Disconnect();
    static bool IsConnected() { return sSocket != INVALID_SOCKET; }
    // 프로그램 종료 시: 연결 해제 + WSACleanup
    static void Shutdown();

    // 매 프레임: recv 반복 → 재조립 → ProcessPacket → FlushSend
    static void Poll();

    // 패킷을 송신 큐에 넣는다. 실제 전송은 Poll()의 FlushSend에서
    static void SendPacket(const void* packet);

    // 마지막 접속 실패 / 연결 끊김 이유 (화면 안내용)
    static const std::wstring& LastError() { return sLastError; }
    // Poll 중에 연결이 끊겼으면 한 번만 true (스스로 Disconnect한 경우는 false)
    static bool ConsumeConnectionLost();

    // ---- 1단계: 에코 테스트 ----
    static void SendEcho(const char* message);
    static const std::string& LastEcho() { return sLastEcho; }
    static int EchoCount() { return sEchoCount; }

private:
    static bool DoRecv();
    static bool FlushSend();
    static void ProcessPacket(unsigned char* packet);

    static SOCKET sSocket;
    static unsigned char sRecvBuf[RECV_BUF_SIZE];
    static int sPrevRemain;
    static std::vector<unsigned char> sSendQueue;
    static bool sWsaStarted;
    static std::string sLastEcho;
    static int sEchoCount;
    static std::wstring sLastError;
    static bool sConnectionLost;
};
