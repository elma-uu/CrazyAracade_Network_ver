#pragma once
#include "Common.h"
#include "protocol.h"

// 서버 연결과 패킷 송수신만 담당한다 (화면·게임 상태는 모른다).
// 소켓은 논블로킹이며, 매 프레임 Poll()에서 받기 → 재조립 → 처리 → 보내기를 한 번에 한다.
// 받은 패킷은 종류별로 GameClient::On~ 에 넘긴다.
class NetworkManager {
public:
    // 블로킹 connect 성공 후 ioctlsocket(FIONBIO)로 논블로킹 전환 (계획서 3.7)
    // 실패하면 false, 이유는 LastError()
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

    // ---- C2S 보내기 ----
    static void SendEcho(const char* message);
    // nameUtf8: UTF-8 닉네임, MAX_NAME_LEN-1 바이트까지 (넘치면 잘림)
    static void SendLogin(const std::string& nameUtf8);
    // 로비 진입·새로고침 때 방 1~8번 요약을 요청 (응답: S2C_RoomList)
    static void SendRoomList();
    // 방 입장 / 나가기 (응답: S2C_RoomEnterResult → S2C_RoomInfo)
    static void SendJoinRoom(int roomId);
    static void SendLeaveRoom();
    // 대기방 안 (결과는 S2C_RoomInfo)
    static void SendSelectMap(MAP_ID map);
    static void SendSelectColor(int color);
    static void SendReady(bool ready);
    static void SendStartGame();
    // 게임 중 방향키 (누르거나 뗄 때만 보낸다, 11/19)
    static void SendInput(INPUT_DIR dir);

private:
    static bool DoRecv();
    static bool FlushSend();
    // false = 크기가 맞지 않는 패킷 → 접속 종료
    static bool ProcessPacket(unsigned char* packet);

    static SOCKET sSocket;
    static unsigned char sRecvBuf[RECV_BUF_SIZE];
    static int sPrevRemain;
    static std::vector<unsigned char> sSendQueue;
    static bool sWsaStarted;
    static std::wstring sLastError;
    static bool sConnectionLost;
};
