#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "protocol.h"
#pragma comment(lib, "ws2_32.lib")

// Wait only until the deadline. A missing server must not hang the test.
bool WaitSocket(SOCKET sock, bool writing, ULONGLONG deadline)
{
    ULONGLONG now = GetTickCount64();
    if (now >= deadline) return false;
    unsigned long remaining = static_cast<unsigned long>(deadline - now);
    timeval timeout = {static_cast<long>(remaining / 1000), static_cast<long>((remaining % 1000) * 1000)};
    fd_set ready, errors;
    FD_ZERO(&ready); FD_SET(sock, &ready);
    FD_ZERO(&errors); FD_SET(sock, &errors);
    int result = select(0, writing ? nullptr : &ready, writing ? &ready : nullptr, &errors, &timeout);
    return result > 0 && !FD_ISSET(sock, &errors) && FD_ISSET(sock, &ready);
}

/* 수정 - 2026-11-01 */
bool ConnectServer(SOCKET sock, const char* ip, unsigned short port)
{
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &address.sin_addr) != 1) {
        std::puts("Invalid IPv4 address."); return false;
    }
    u_long nonblocking = 1;
    if (ioctlsocket(sock, FIONBIO, &nonblocking) == SOCKET_ERROR) return false;
    if (connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK || !WaitSocket(sock, true, GetTickCount64() + 5000)) {
            std::puts("Connect failed or timed out."); return false;
        }
        int error = 0, length = sizeof(error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length) == SOCKET_ERROR || error != 0)
            return false;
    }
    BOOL noDelay = TRUE;
    return setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&noDelay), sizeof(noDelay)) != SOCKET_ERROR;
}

/* 수정 - 2026-11-01 */
bool SendBytes(SOCKET sock, const void* bytes, int total, bool byteMode)
{
    const char* data = static_cast<const char*>(bytes);
    int offset = 0, calls = 0;
    ULONGLONG deadline = GetTickCount64() + 5000;
    while (offset < total) {
        if (!WaitSocket(sock, true, deadline)) { std::puts("Send timeout/error."); return false; }
        /* 수정 - 2026-11-03 */
        int length = byteMode ? 1 : total - offset;
        int sent = send(sock, data + offset, length, 0);
        if (sent == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) continue;
            std::printf("send failed: %d\n", WSAGetLastError()); return false;
        }
        if (sent == 0) return false;
        offset += sent; ++calls;
        if (byteMode && offset < total) Sleep(10);
    }
    std::printf("TX: %d bytes, %d successful send calls (%s).\n", offset, calls, byteMode ? "1 byte per call" : "normal");
    return true;
}

/* 수정 - 2026-11-02 */
bool ReceiveEcho(SOCKET sock, const C2S_Echo& expected)
{
    unsigned char buffer[sizeof(S2C_Echo)] = {};
    int used = 0;
    ULONGLONG deadline = GetTickCount64() + 5000;
    for (;;) {
        if (!WaitSocket(sock, false, deadline)) { std::puts("Receive timeout/error."); return false; }
        int received = recv(sock, reinterpret_cast<char*>(buffer) + used, static_cast<int>(sizeof(buffer)) - used, 0);
        if (received == 0) { std::printf("Disconnected with %d incomplete bytes.\n", used); return false; }
        if (received == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) continue;
            std::printf("recv failed: %d\n", WSAGetLastError()); return false;
        }
        used += received;
        std::printf("RX: %d bytes, accumulated %d.\n", received, used);
        if (used < sizeof(PACKET_HEADER)) continue;
        PACKET_HEADER header;
        std::memcpy(&header, buffer, sizeof(header));
        // Read at most one reply here. The next reply stays in the socket.
        if (header.size != sizeof(S2C_Echo) || header.type != S2C_ECHO) {
            std::printf("Unexpected header: size=%u type=%u.\n", header.size, header.type); return false;
        }
        if (used < header.size) continue;
        S2C_Echo echo;
        std::memcpy(&echo, buffer, sizeof(echo));
        if (std::memcmp(echo.message, expected.message, sizeof(echo.message)) != 0) {
            std::puts("Echo content mismatch."); return false;
        }
        std::printf("PASS: S2C_Echo, size=%u, message=\"%.*s\"\n", echo.size, 64, echo.message);
        return true;
    }
}

// The plan requires a disconnect when size is outside [3, 1024].
/* 수정 - 2026-11-05 */
bool ExpectDisconnect(SOCKET sock, unsigned short size)
{
    PACKET_HEADER header = {size, C2S_ECHO};
    if (!SendBytes(sock, &header, sizeof(header), false)) return false;
    if (!WaitSocket(sock, false, GetTickCount64() + 5000)) {
        std::puts("FAIL: no disconnect observed before deadline."); return false;
    }
    char byte;
    int result = recv(sock, &byte, 1, 0);
    if (result == 0 || (result == SOCKET_ERROR && WSAGetLastError() == WSAECONNRESET)) {
        std::printf("PASS: peer disconnected after size=%u.\n", size);
        return true;
    }
    std::puts("FAIL: expected peer disconnect without reply.");
    return false;
}

/* 수정 - 2026-10-31 */
int main(int argc, char* argv[])
{
    const char* mode = argc >= 3 ? argv[2] : "";
    bool byteMode = std::strcmp(mode, "byte") == 0;
    bool bundle = std::strcmp(mode, "bundle") == 0;
    bool small = std::strcmp(mode, "bad-small") == 0;
    bool large = std::strcmp(mode, "bad-large") == 0;
    if (argc < 3 || argc > 5 || (!byteMode && !bundle && !small && !large && std::strcmp(mode, "normal") != 0)) {
        std::puts("Usage: DummyClient <IPv4> <normal|byte|bundle|bad-small|bad-large> [message] [port]"); return 1;
    }
    const char* message = argc >= 4 ? argv[3] : "gyudong echo test";
    if (std::strlen(message) >= 64) { std::puts("Message must be at most 63 bytes."); return 1; }
    unsigned short port = SERVER_PORT;
    if (argc == 5) {
        char* end = nullptr; long value = std::strtol(argv[4], &end, 10);
        if (!argv[4][0] || *end || value < 1 || value > 65535) { std::puts("Invalid port."); return 1; }
        port = static_cast<unsigned short>(value);
    }
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return 1;
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    bool ok = false;
    if (sock != INVALID_SOCKET && ConnectServer(sock, argv[1], port)) {
        std::printf("Connected: %s:%u\n", argv[1], port);
        if (small || large) {
            ok = ExpectDisconnect(sock, small ? 2 : 1025);
        } else {
            C2S_Echo packets[3] = {};
            /* 수정 - 2026-11-04 */
            int count = bundle ? 3 : 1;
            for (int i = 0; i < count; ++i) {
                packets[i].size = sizeof(C2S_Echo);
                packets[i].type = C2S_ECHO;
                if (bundle)
                    std::snprintf(packets[i].message, sizeof(packets[i].message), "%d:%.61s", i + 1, message);
                else
                    std::memcpy(packets[i].message, message, std::strlen(message));
            }
            ok = SendBytes(sock, packets, count * static_cast<int>(sizeof(C2S_Echo)), byteMode);
            for (int i = 0; ok && i < count; ++i)
                ok = ReceiveEcho(sock, packets[i]);
            if (ok && bundle) std::puts("PASS: 3 ordered echoes for 201 bundled bytes.");
        }
    }
    if (sock != INVALID_SOCKET) closesocket(sock);
    WSACleanup();
    if (ok) return 0;
    return 1;
}
