#include "ResourceManager.h"
#include "resource2.h"

HBITMAP ResourceManager::sBitmaps[static_cast<int>(BitmapID::Count)] = {};

void ResourceManager::LoadAll(HINSTANCE hInst)
{
    struct Entry { BitmapID id; int resource; };
    static const Entry entries[] = {
        { BitmapID::Login,           IDB_BITMAP_GAME_LOGIN },
        { BitmapID::SelectStage,     IDB_BITMAP_SELECT_STAGE },
        { BitmapID::P1Check,         IDB_BITMAP_P1_CHECK },
        { BitmapID::P2Check,         IDB_BITMAP_P2_CHECK },
        { BitmapID::GameUI,          IDB_BITMAP_GAMEUI },
        { BitmapID::GameEnd,         IDB_BITMAP_GAME_END },

        { BitmapID::BombSet,         IDB_BITMAP_BOMB_CENTER },
        { BitmapID::BombCenter,      IDB_BITMAP_BOMB_POP },
        { BitmapID::BombUp,          IDB_BITMAP_BOMB_UP2 },
        { BitmapID::BombDown,        IDB_BITMAP_BOMB_DOWN2 },
        { BitmapID::BombLeft,        IDB_BITMAP_BOMB_LEFT2 },
        { BitmapID::BombRight,       IDB_BITMAP_BOMB_RIGHT2 },

        { BitmapID::ForestTileMax,   IDB_BITMAP_FOREST_TILE_MAX },
        { BitmapID::ForestObstacle1, IDB_BITMAP_FOREST_OBSTACLE_1 },
        { BitmapID::ForestObstacle2, IDB_BITMAP_FOREST_OBSTACLE_2 },
        { BitmapID::ForestObstacle3, IDB_BITMAP_FOREST_OBSTACLE_3 },
        { BitmapID::ForestObstacle4, IDB_BITMAP_FOREST_OBSTACLE_4 },
        { BitmapID::ForestObstacle5, IDB_BITMAP_FOREST_OBSTACLE_5 },
        { BitmapID::ForestTree,      IDB_BITMAP_BIG_TREE },
        { BitmapID::ForestApple,     IDB_BITMAP_Forest_APPLE },
        { BitmapID::ForestAppleBox,  IDB_BITMAP_FOREST_APPLEBOX },

        { BitmapID::CityTileMax,     IDB_BITMAP_TILE_CITY },
        { BitmapID::CityTrain,       IDB_BITMAP_CITY_TRAIN },
        { BitmapID::CityBuilding1,   IDB_BITMAP_CITY_BUILDING1 },
        { BitmapID::CityBuilding2,   IDB_BITMAP_CITY_BUILDING2 },
        { BitmapID::CityBuilding3,   IDB_BITMAP_CITY_BUILDING3 },
        { BitmapID::CityObstacles,   IDB_BITMAP_CITY_OBJECTS },
        { BitmapID::CityTrashbox,    IDB_BITMAP_CITY_TRASHBOX },
        { BitmapID::CityBlinkerOff,  IDB_BITMAP_CITY_BLINKER },
        { BitmapID::CityBlinkerOn,   IDB_BITMAP_BLINKER_ON },
        { BitmapID::CityClock,       IDB_BITMAP_CITY_CLOCK },

        { BitmapID::IceTileMax,      IDB_BITMAP_ICE_TILE_MAX },
        { BitmapID::IceBlock1,       IDB_BITMAP_ICE_BLOCK1 },
        { BitmapID::IceBlock2,       IDB_BITMAP_ICE_BLOCK2 },
        { BitmapID::IceObstacles,    IDB_BITMAP_ICE_OBSTACLES },
        { BitmapID::Heart,           IDB_BITMAP_HEART },

        { BitmapID::Player,          IDB_BITMAP_PLAYER },
        { BitmapID::PlayerMask,      IDB_BITMAP_PLAYER_MASK },

        { BitmapID::ItemPower,       IDB_BITMAP_ITEM_POWER },
        { BitmapID::ItemBombCount,   IDB_BITMAP_ITEM_BOMBS },
        { BitmapID::ItemSpeed,       IDB_BITMAP_ITEM_SPEED },
    };
    static_assert(sizeof(entries) / sizeof(entries[0]) == static_cast<size_t>(BitmapID::Count),
        "BitmapID마다 리소스를 하나씩 등록해야 합니다");

    for (const Entry& e : entries)
        sBitmaps[static_cast<int>(e.id)] = LoadBitmap(hInst, MAKEINTRESOURCE(e.resource));
}

void ResourceManager::UnloadAll()
{
    for (HBITMAP& bitmap : sBitmaps) {
        if (bitmap) DeleteObject(bitmap);
        bitmap = nullptr;
    }
}
