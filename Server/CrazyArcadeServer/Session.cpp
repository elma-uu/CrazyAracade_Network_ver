#include "Session.h"
#include "Room.h"
#include "Server.h"
#include <cstring>
#include <iostream>

void SESSION::Init(int id, SOCKET sock)
{
    m_id = id;
    m_socket = sock;
    m_state = ST_CONNECTED;
    m_name[0] = '\0';
    m_roomId = -1;
    m_slot = -1;
    m_prevRemain = 0;
    m_sendQueue.clear();
}

// 다음 접속자가 이 자리를 그대로 쓰므로, 이전 접속자의 흔적을 모두 지운다.
// (받다 만 조각이 남으면 새 접속자의 첫 패킷 앞에 붙고, 못 보낸 데이터가 남으면 새 접속자에게 간다)
void SESSION::Reset()
{
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    m_name[0] = '\0';              // 나간 사람의 닉네임은 다시 쓸 수 있게 된다
    m_roomId = -1;
    m_slot = -1;
    m_prevRemain = 0;
    m_sendQueue.clear();
    m_sendQueue.shrink_to_fit();   // 큐가 크게 자랐던 자리의 메모리도 돌려준다
    m_state = ST_FREE;
}

// 논블로킹 send는 일부만 보내거나 WSAEWOULDBLOCK을 돌려줄 수 있어서
// 바로 보내지 않고 세션마다 있는 송신 큐에 쌓아 둔다.
void SESSION::SendPacket(const void* packet)
{
    auto p    = static_cast<const unsigned char*>(packet);
    int  size = reinterpret_cast<const PACKET_HEADER*>(p)->size;
    m_sendQueue.insert(m_sendQueue.end(), p, p + size);
}

bool SESSION::FlushSend()
{
    while (!m_sendQueue.empty()) {
        auto data = reinterpret_cast<const char*>(m_sendQueue.data());
        int  ret  = send(m_socket, data, static_cast<int>(m_sendQueue.size()), 0);

        if (ret == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {   // 진짜 에러 → 접속 종료
                std::cout << "[" << m_id << "] ";
                PrintSocketError("send", err);
                return false;
            }
            break;                         // 지금은 못 보냄 → 다음 루프에서
        }

        // 실제로 보낸 만큼만 큐에서 지운다
        m_sendQueue.erase(m_sendQueue.begin(), m_sendQueue.begin() + ret);
    }

    // 큐가 64KB를 넘으면 데이터를 받지 못하는 클라이언트로 보고 끊는다
    if (m_sendQueue.size() > static_cast<size_t>(MAX_SEND_QUEUE)) {
        std::cout << "[" << m_id << "] send queue overflow ("
                  << m_sendQueue.size() << " bytes)" << std::endl;
        return false;
    }
    return true;
}

// 한 루프에서 세션 하나가 recv할 수 있는 최대 횟수 (최대 4 × 4KB = 16KB).
// 쉬지 않고 보내는 클라이언트 하나 때문에 메인 루프가 그 세션에 갇혀
// 다른 세션의 처리와 FlushSend가 멈추는 것을 막는다. 남은 데이터는 다음 루프에서 받는다.
constexpr int MAX_RECV_PER_LOOP = 4;

// TCP는 보낸 단위 그대로 오지 않는다 (여러 패킷이 한 번에 / 한 패킷이 여러 번에).
// 받은 데이터를 m_recvBuf에 모아 두고, 맨 앞 헤더의 size만큼 다 모였을 때만 처리한다.
bool SESSION::DoRecv()
{
    for (int i = 0; i < MAX_RECV_PER_LOOP; ++i) {
        // 지난번에 남은 조각 뒤에 이어서 받는다
        char* buf   = reinterpret_cast<char*>(m_recvBuf) + m_prevRemain;
        int   space = RECV_BUF_SIZE - m_prevRemain;
        int   ret   = recv(m_socket, buf, space, 0);

        if (ret == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK)   // 지금은 받을 데이터 없음 (정상)
                return true;
            std::cout << "[" << m_id << "] ";
            PrintSocketError("recv", err);
            return false;
        }
        if (ret == 0)                    // 상대가 연결 종료
            return false;

        unsigned char* packet_start = m_recvBuf;
        int remain_data = m_prevRemain + ret;

        // 헤더(3바이트)가 다 왔는지부터 확인
        while (remain_data >= static_cast<int>(sizeof(PACKET_HEADER))) {
            auto header = reinterpret_cast<PACKET_HEADER*>(packet_start);
            int packet_size = header->size;

            // 말이 안 되는 size → 잘못된 패킷, 접속 종료
            if (packet_size < static_cast<int>(sizeof(PACKET_HEADER)) ||
                packet_size > MAX_PACKET_SIZE) {
                std::cout << "[" << m_id << "] bad packet size " << packet_size << std::endl;
                return false;
            }

            // 패킷이 아직 덜 옴 → 다음 recv에서 이어 붙임
            if (packet_size > remain_data)
                break;

            if (!ProcessPacket(packet_start))   // 잘못된 패킷 → 접속 종료
                return false;
            packet_start += packet_size;
            remain_data  -= packet_size;
        }

        // 남은 조각을 버퍼 앞으로 옮겨 둔다
        m_prevRemain = remain_data;
        if (remain_data > 0)
            memmove(m_recvBuf, packet_start, remain_data);
    }
    return true;   // 이번 루프 몫은 다 받음
}

bool SESSION::ProcessPacket(unsigned char* packet)
{
    auto header = reinterpret_cast<PACKET_HEADER*>(packet);

    // 종류는 맞는데 크기가 다르면 struct 밖의 버퍼를 읽게 된다 → 잘못된 패킷
    int expected = GetPacketSize(header->type);
    if (expected == 0) {
        // 모르는 종류 → 로그만 남기고 무시
        std::cout << "[" << m_id << "] unknown packet type=" << static_cast<int>(header->type)
                  << " size=" << header->size << std::endl;
        return true;
    }
    if (header->size != expected) {
        std::cout << "[" << m_id << "] bad size for type " << static_cast<int>(header->type)
                  << ": " << header->size << " (expected " << expected << ")" << std::endl;
        return false;
    }

    switch (header->type) {
    case C2S_ECHO:
        OnEcho(reinterpret_cast<C2S_Echo*>(packet));
        break;
    case C2S_LOGIN:
        OnLogin(reinterpret_cast<C2S_Login*>(packet));
        break;
    case C2S_JOIN_ROOM:
        OnJoinRoom(reinterpret_cast<C2S_JoinRoom*>(packet));
        break;
    default:
        // 아직 처리하지 않는 패킷 → 로그만 남기고 무시
        std::cout << "[" << m_id << "] unhandled packet type=" << static_cast<int>(header->type)
                  << " size=" << header->size << std::endl;
        break;
    }
    return true;
}

// 1단계 에코: 받은 문자열을 그대로 돌려준다
void SESSION::OnEcho(const C2S_Echo* p)
{
    S2C_Echo reply{};
    reply.size = sizeof(reply);
    reply.type = S2C_ECHO;
    memcpy(reply.message, p->message, sizeof(reply.message));
    SendPacket(&reply);

    // 로그 출력용 복사본: 클라가 '\0' 없이 64바이트를 꽉 채워 보내도 안전하게
    char text[sizeof(p->message) + 1]{};
    memcpy(text, p->message, sizeof(p->message));
    std::cout << "[" << m_id << "] echo: " << text << std::endl;
}

namespace {
    // 닉네임 검사. 문제가 없으면 nullptr, 있으면 실패 이유를 돌려준다.
    // - '\0'이 16바이트 안에 있어야 한다 (UTF-8 15바이트까지, 한글 5글자)
    // - 빈 이름 불가
    // - 공백·제어 문자 불가 (전적 파일이 "닉네임 승 패"를 공백으로 구분하기 때문)
    const char* CheckName(const char (&name)[MAX_NAME_LEN])
    {
        size_t len = strnlen(name, MAX_NAME_LEN);
        if (len == MAX_NAME_LEN) return "too long";
        if (len == 0)            return "empty";
        for (size_t i = 0; i < len; ++i) {
            unsigned char c = static_cast<unsigned char>(name[i]);
            if (c <= 0x20 || c == 0x7F) return "space or control character";
        }
        return nullptr;
    }

    // 다른 세션이 이미 같은 닉네임으로 로그인해 있는지 (대소문자 구분, 바이트 그대로 비교)
    bool IsNameInUse(const char* name, int exceptId)
    {
        for (const SESSION& s : g_sessions) {
            if (s.GetId() == exceptId || !s.IsLoggedIn())
                continue;
            if (strcmp(s.GetName(), name) == 0)
                return true;
        }
        return false;
    }
}

// 로그인: 닉네임 검사 → 중복 검사 → 닉네임 저장 → S2C_LoginResult → 로비(ST_LOBBY)
// 실패하면 ST_CONNECTED 그대로 두어 다른 닉네임으로 다시 시도할 수 있게 한다.
void SESSION::OnLogin(const C2S_Login* p)
{
    if (m_state != ST_CONNECTED) {
        // 이미 로비 이후 상태 → 응답 없이 무시
        std::cout << "[" << m_id << "] login ignored: already logged in as "
                  << m_name << std::endl;
        return;
    }

    if (const char* reason = CheckName(p->name)) {
        std::cout << "[" << m_id << "] login failed: " << reason << std::endl;
        SendLoginResult(false);
        return;
    }

    if (IsNameInUse(p->name, m_id)) {
        std::cout << "[" << m_id << "] login failed: duplicate name " << p->name << std::endl;
        SendLoginResult(false);
        return;
    }

    memcpy(m_name, p->name, MAX_NAME_LEN);   // CheckName에서 '\0'이 있는 것을 확인함
    m_state = ST_LOBBY;
    SendLoginResult(true);
    std::cout << "[" << m_id << "] login ok: " << m_name << " -> lobby" << std::endl;
}

void SESSION::SendLoginResult(bool success)
{
    S2C_LoginResult p{};
    p.size    = sizeof(p);
    p.type    = S2C_LOGIN_RESULT;
    p.success = success;
    p.id      = success ? m_id : -1;
    // 전적은 RecordDB(11/28)를 만든 뒤 채운다 (11/30: 로그인 전적 전송)
    p.win     = 0;
    p.lose    = 0;
    SendPacket(&p);
}

// 로비에서 방 선택 → 입장 결과. 성공하면 ST_ROOM
// (방 안 전원에게 S2C_RoomInfo는 11/13, 로비 방 목록 갱신은 11/18)
void SESSION::OnJoinRoom(const C2S_JoinRoom* p)
{
    if (m_state != ST_LOBBY) {
        // 로그인 전이거나 이미 방/게임 중 → 응답 없이 무시
        std::cout << "[" << m_id << "] join room ignored: not in lobby" << std::endl;
        return;
    }

    int roomId = p->roomId;
    if (!RoomManager::IsValidRoomId(roomId)) {
        // 클라 화면에는 1~8번만 있으므로 그 밖의 번호는 잘못된 요청 → 무시
        std::cout << "[" << m_id << "] join room ignored: invalid room " << roomId << std::endl;
        return;
    }

    int slot = -1;
    unsigned char failReason = ENTER_OK;
    if (!g_roomManager.JoinRoom(m_id, roomId, slot, failReason)) {
        std::cout << "[" << m_id << "] join room " << roomId << " failed: "
                  << (failReason == ENTER_PLAYING ? "playing" : "full") << std::endl;
        SendRoomEnterResult(false, roomId, -1, failReason);
        return;
    }

    m_roomId = roomId;
    m_slot   = slot;
    m_state  = ST_ROOM;
    SendRoomEnterResult(true, roomId, slot, ENTER_OK);
    std::cout << "[" << m_id << "] joined room " << roomId << " slot " << slot
              << (g_roomManager.Get(roomId).IsHost(m_id) ? " (host)" : "") << std::endl;
}

void SESSION::SendRoomEnterResult(bool success, int roomId, int slot, unsigned char failReason)
{
    S2C_RoomEnterResult p{};
    p.size       = sizeof(p);
    p.type       = S2C_ROOM_ENTER_RESULT;
    p.success    = success;
    p.roomId     = static_cast<unsigned char>(roomId);
    p.slot       = success ? static_cast<unsigned char>(slot) : 0xFF;   // 실패면 0xFF
    p.failReason = static_cast<ENTER_FAIL>(failReason);
    SendPacket(&p);
}
