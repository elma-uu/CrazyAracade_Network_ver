#pragma once
#include <cstdint>

/* 수정 - 2026-11-10 */
enum class MapId : std::uint8_t { Forest = 1, City, Ice };
constexpr std::uint8_t S2C_MAP_INFO = 17;
constexpr int MAP_TILES = 20;

#pragma pack(push, 1)
struct S2C_MapInfo {
    std::uint16_t size;
    std::uint8_t type;
    MapId mapId;
    std::uint8_t obstacle[MAP_TILES][MAP_TILES];
    std::uint8_t obstacleType[MAP_TILES][MAP_TILES];
};
#pragma pack(pop)
static_assert(sizeof(S2C_MapInfo) == 804);
