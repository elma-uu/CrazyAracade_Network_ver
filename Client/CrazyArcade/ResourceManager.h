#pragma once
#include "Common.h"

enum class BitmapID {
    // UI
    Login,
    SelectStage,
    P1Check,
    P2Check,
    GameUI,
    GameEnd,
    // 물풍선
    BombSet,
    BombCenter,
    BombUp,
    BombDown,
    BombLeft,
    BombRight,
    // 숲
    ForestTileMax,
    ForestObstacle1,
    ForestObstacle2,
    ForestObstacle3,
    ForestObstacle4,
    ForestObstacle5,
    ForestTree,
    ForestApple,
    ForestAppleBox,
    // 도시
    CityTileMax,
    CityTrain,
    CityBuilding1,
    CityBuilding2,
    CityBuilding3,
    CityObstacles,
    CityTrashbox,
    CityBlinkerOff,
    CityBlinkerOn,
    CityClock,
    // 얼음
    IceTileMax,
    IceBlock1,
    IceBlock2,
    IceObstacles,
    Heart,
    // 플레이어 (색상은 Player가 자기 복사본을 만들어 입힌다)
    Player,
    PlayerMask,
    // 아이템
    ItemPower,
    ItemBombCount,
    ItemSpeed,
    // 로비
    LobbyBg,        // 800x600 배경 (글자는 코드로 그림)
    LobbyTiles,     // 열린 칸 / X 칸 (각 101x101)
    LobbyMaps,      // 숲·도시·얼음 아이콘 64px(y=0) / 58px(y=64) / 44px(y=122)
    LobbySprites,   // ? 아이콘·왕관·숫자. 32비트 알파(미리 곱함) → Gfx::DrawAlpha로 그린다
    // 대기방
    RoomBg,         // 800x600 배경 (위 줄 4칸 열림, 맵 선택 칸 3개)
    RoomSprites,    // 배찌 7색(86x86) + 체크 표시. 32비트 알파

    Count
};

class ResourceManager {
public:
    static void LoadAll(HINSTANCE hInst);
    static void UnloadAll();
    static HBITMAP Get(BitmapID id) { return sBitmaps[static_cast<int>(id)]; }

private:
    static HBITMAP sBitmaps[static_cast<int>(BitmapID::Count)];
};
