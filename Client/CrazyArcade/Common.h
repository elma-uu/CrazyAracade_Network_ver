#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Windows.h가 옛 winsock.h를 먼저 끌어오면 WinSock2.h와 충돌하므로 반드시 WinSock2.h를 먼저 포함한다
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <cmath>

namespace Config {
    // 백버퍼(=클라이언트 영역) 크기
    constexpr int ScreenWidth = 1500;
    constexpr int ScreenHeight = 900;

    // 맵: 20x20 타일, 한 칸 40px, 화면상 좌상단 위치
    constexpr int MapTiles = 20;
    constexpr int TileSize = 40;
    constexpr int TileHalf = TileSize / 2;
    constexpr int MapOriginX = 325;
    constexpr int MapOriginY = 20;

    // 게임 로직은 12ms 고정 스텝으로 돈다 (기존 이동 타이머 간격)
    constexpr int FixedStepMs = 12;

    constexpr COLORREF ColorKey = RGB(255, 0, 255);
}

enum class Dir { Up, Down, Left, Right, None };
