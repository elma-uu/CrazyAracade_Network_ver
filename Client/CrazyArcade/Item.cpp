#include "Item.h"
#include "Gfx.h"
#include "Player.h"
#include "StageScene.h"

void Item::Render(HDC hdc, int cx, int cy) const
{
    SIZE src = GetIconSize();
    Gfx::Draw(hdc, GetIcon(), cx - Config::TileHalf, cy - Config::TileHalf,
        Config::TileSize, Config::TileSize, 0, 0, src.cx, src.cy);
}

const Item* Item::Get(ItemType type)
{
    static const PowerItem power;
    static const BombCountItem bombCount;
    static const SpeedItem speed;
    static const ClockItem clock;

    switch (type) {
    case ItemType::Power:     return &power;
    case ItemType::BombCount: return &bombCount;
    case ItemType::Speed:     return &speed;
    case ItemType::Clock:     return &clock;
    default:                  return nullptr;
    }
}

ItemType Item::Random(bool includeClock)
{
    int count = includeClock ? 4 : 3;
    return static_cast<ItemType>(rand() % count + 1);
}

void PowerItem::Apply(Player& player, StageScene&) const
{
    player.SetBombRange(player.GetBombRange() + 1);
}

void BombCountItem::Apply(Player& player, StageScene&) const
{
    player.SetMaxBombs(player.GetMaxBombs() + 1);
}

void SpeedItem::Apply(Player& player, StageScene&) const
{
    if (player.GetSpeed() < 1.5f) player.SetSpeed(player.GetSpeed() + 0.1f);
}

void ClockItem::Apply(Player& player, StageScene& stage) const
{
    stage.AddTime(player, 5);
}
