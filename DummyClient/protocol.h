#pragma once
#include <cstdint>

/* 수정 - 2026-10-31 */
// Plan's echo subset. Replace with the team's shared protocol.h when available.
constexpr unsigned short SERVER_PORT = 9000;
enum PACKET_TYPE : unsigned char { C2S_ECHO = 1, S2C_ECHO = 2 };
#pragma pack(push, 1)
struct PACKET_HEADER { std::uint16_t size; PACKET_TYPE type; };
struct C2S_Echo { std::uint16_t size; PACKET_TYPE type; char message[64]; };
struct S2C_Echo { std::uint16_t size; PACKET_TYPE type; char message[64]; };
#pragma pack(pop)
static_assert(sizeof(PACKET_HEADER) == 3, "header size");
static_assert(sizeof(C2S_Echo) == 67 && sizeof(S2C_Echo) == 67, "echo size");
