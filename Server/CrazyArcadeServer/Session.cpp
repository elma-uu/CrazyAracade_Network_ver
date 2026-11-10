#include "Session.h"
#include "Server.h"
#include <cstring>
#include <iostream>

void SESSION::Init(int id, SOCKET sock)
{
    m_id = id;
    m_socket = sock;
    m_state = ST_CONNECTED;
    m_prevRemain = 0;
    m_sendQueue.clear();
}

void SESSION::Close()
{
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    m_sendQueue.clear();
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

// TCP는 보낸 단위 그대로 오지 않는다 (여러 패킷이 한 번에 / 한 패킷이 여러 번에).
// 받은 데이터를 m_recvBuf에 모아 두고, 맨 앞 헤더의 size만큼 다 모였을 때만 처리한다.
bool SESSION::DoRecv()
{
    while (true) {
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

            ProcessPacket(packet_start);
            packet_start += packet_size;
            remain_data  -= packet_size;
        }

        // 남은 조각을 버퍼 앞으로 옮겨 둔다
        m_prevRemain = remain_data;
        if (remain_data > 0)
            memmove(m_recvBuf, packet_start, remain_data);
    }
}

void SESSION::ProcessPacket(unsigned char* packet)
{
    auto header = reinterpret_cast<PACKET_HEADER*>(packet);

    switch (header->type) {
    case C2S_ECHO:
        OnEcho(reinterpret_cast<C2S_Echo*>(packet));
        break;
    default:
        // 아직 처리하지 않는 패킷 → 로그만 남기고 무시
        std::cout << "[" << m_id << "] unknown packet type=" << static_cast<int>(header->type)
                  << " size=" << header->size << std::endl;
        break;
    }
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
