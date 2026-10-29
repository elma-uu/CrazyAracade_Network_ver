#pragma once
#include "Common.h"
#include "ResourceManager.h"

enum class ItemType { None = 0, Power, BombCount, Speed, Clock, Count };

class Player;
class StageScene;

// 아이템 효과는 상태가 없으므로 타입마다 인스턴스 하나만 두고 공유한다 (Item::Get)
class Item {
public:
    virtual ~Item() = default;

    virtual ItemType GetType() const = 0;
    virtual void Apply(Player& player, StageScene& stage) const = 0;

    // (cx, cy)를 중심으로 한 칸 크기로 그린다
    void Render(HDC hdc, int cx, int cy) const;

    static const Item* Get(ItemType type);
    // Power/BombCount/Speed 중 하나, includeClock이면 Clock 포함
    static ItemType Random(bool includeClock = false);

protected:
    virtual BitmapID GetIcon() const = 0;
    virtual SIZE GetIconSize() const { return { 38, 38 }; }
};

// 물줄기 길이 +1
class PowerItem : public Item {
public:
    ItemType GetType() const override { return ItemType::Power; }
    void Apply(Player& player, StageScene& stage) const override;
protected:
    BitmapID GetIcon() const override { return BitmapID::ItemPower; }
};

// 동시에 놓을 수 있는 물풍선 +1
class BombCountItem : public Item {
public:
    ItemType GetType() const override { return ItemType::BombCount; }
    void Apply(Player& player, StageScene& stage) const override;
protected:
    BitmapID GetIcon() const override { return BitmapID::ItemBombCount; }
};

// 이동 속도 +0.1 (최대 1.5)
class SpeedItem : public Item {
public:
    ItemType GetType() const override { return ItemType::Speed; }
    void Apply(Player& player, StageScene& stage) const override;
protected:
    BitmapID GetIcon() const override { return BitmapID::ItemSpeed; }
};

// 남은 시간 +5초 (시간제 스테이지에서만 의미 있음)
class ClockItem : public Item {
public:
    ItemType GetType() const override { return ItemType::Clock; }
    void Apply(Player& player, StageScene& stage) const override;
protected:
    BitmapID GetIcon() const override { return BitmapID::CityClock; }
    SIZE GetIconSize() const override { return { 40, 46 }; }
};
