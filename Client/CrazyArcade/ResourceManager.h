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
