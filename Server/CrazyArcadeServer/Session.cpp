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
}

void SESSION::Close()
{
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    m_state = ST_FREE;
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

    // 11/5부터 패킷 종류별 처리. 지금은 재조립 확인용 로그만 남긴다.
    std::cout << "[" << m_id << "] packet type=" << static_cast<int>(header->type)
              << " size=" << header->size << std::endl;
}
