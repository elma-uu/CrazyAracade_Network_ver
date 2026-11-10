#include "StageScene.h"
#include "SceneManager.h"
#include "AudioManager.h"
#include "GameData.h"
#include "Input.h"
#include "Gfx.h"

namespace {
    const KeyBinding kKeyBindings[2] = {
        { 'W', 'S', 'A', 'D', VK_SPACE },
        { VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_RETURN },
    };
}

StageScene::StageScene()
{
    for (int i = 0; i < PlayerCount; ++i)
        mPlayers[i] = std::make_unique<Player>(i, kKeyBindings[i], GameData::playerColor[i], mMap);
}

void StageScene::OnEnter()
{
    Audio::StopMusic();
    Audio::PlayEffect("GameStart.wav");

    BuildMap(mMap);
    for (int i = 0; i < PlayerCount; ++i) {
        POINT spawn = GetSpawnTile(i);
        mPlayers[i]->Spawn(spawn.x, spawn.y);
    }
}

void StageScene::Update(float)
{
    if (IsGameOver()) {
        if (Input::GetKeyDown('R')) SceneManager::ChangeScene(SceneType::Title);
        return;
    }

    for (auto& player : mPlayers) {
        player->HandleInput();
        if (player->WantsToPlaceBomb() && CanPlaceBomb(*player)) PlaceBomb(*player);
    }
}

void StageScene::FixedUpdate()
{
    if (IsGameOver()) return;

    for (auto& player : mPlayers) player->FixedUpdate();
    for (auto& player : mPlayers) PickUpItem(*player);

    UpdateBombs();
    if (IsGameOver()) return;

    OnFixedUpdate();
}

bool StageScene::CanPlaceBomb(const Player& player) const
{
    return CountBombs(player) < player.GetMaxBombs();
}

int StageScene::CountBombs(const Player& owner) const
{
    return static_cast<int>(std::count_if(mBombs.begin(), mBombs.end(),
        [&](const std::unique_ptr<Bomb>& bomb) { return &bomb->GetOwner() == &owner; }));
}

void StageScene::PlaceBomb(Player& player)
{
    // 이미 물풍선이 있는 칸에는 겹쳐 놓지 않는다
    if (mMap.At(player.GetRow(), player.GetCol()).obstacle == Obstacle::Bomb) return;
    mBombs.push_back(std::make_unique<Bomb>(player, mMap));
    Audio::PlayEffect("Ballon.wav");
}

void StageScene::UpdateBombs()
{
    for (auto& bomb : mBombs) {
        bomb->FixedUpdate();
        if (bomb->JustExploded()) {
            Audio::PlayEffect("Bubble.wav");
            Player& owner = bomb->GetOwner();
            bomb->Explode([&](Tile& tile) { OnFlameBlocked(tile, owner); });
        }
    }

    // 물줄기 판정: 자기 물풍선에는 맞지 않는다
    for (auto& bomb : mBombs) {
        if (!bomb->IsExploding() || bomb->IsDead()) continue;
        Player& attacker = bomb->GetOwner();
        Player& victim = GetOpponent(attacker);
        if (!bomb->TryHit(victim)) continue;

        if (!victim.IsHit()) Audio::PlayEffect("Die.wav");
        victim.Hit();
        OnPlayerHit(victim, attacker);
        if (IsGameOver()) break;
    }

    mBombs.erase(std::remove_if(mBombs.begin(), mBombs.end(),
        [](const std::unique_ptr<Bomb>& bomb) { return bomb->IsDead(); }), mBombs.end());
}

void StageScene::PickUpItem(Player& player)
{
    Tile& tile = mMap.At(player.GetRow(), player.GetCol());
    const Item* item = Item::Get(tile.item);
    if (!item) return;

    item->Apply(player, *this);
    player.OnItemPicked(tile.item);
    tile.item = ItemType::None;
    Audio::PlayEffect("Acting.wav");
}

void StageScene::EndGame(int winnerIndex)
{
    if (IsGameOver()) return;
    mWinner = winnerIndex;
    Audio::StopMusic();
    Audio::PlayEffect("Win.wav");
}

void StageScene::RenderPlayersAt(HDC hdc, int row, int col) const
{
    for (const auto& player : mPlayers)
        if (player->GetRow() == row && player->GetCol() == col) player->Render(hdc);
}

void StageScene::Render(HDC hdc)
{
    RenderCommonHud(hdc);
    RenderBackground(hdc);
    for (const auto& bomb : mBombs) bomb->Render(hdc);
    RenderMap(hdc);
    RenderHud(hdc);
    if (IsGameOver()) RenderResult(hdc);
}

void StageScene::RenderCommonHud(HDC hdc) const
{
    Gfx::DrawOpaque(hdc, BitmapID::GameUI, 0, 0, 1450, 900, 0, 0, 1040, 780);

    const int portraitX[PlayerCount] = { 175, 1195 };
    const int iconX[PlayerCount] = { 50, 1200 };
    struct HudItem { ItemType type; BitmapID icon; int y; };
    const HudItem hudItems[] = {
        { ItemType::Power,     BitmapID::ItemPower,     700 },
        { ItemType::BombCount, BitmapID::ItemBombCount, 740 },
        { ItemType::Speed,     BitmapID::ItemSpeed,     780 },
    };

    for (int i = 0; i < PlayerCount; ++i) {
        mPlayers[i]->RenderPortrait(hdc, portraitX[i], 150);
        for (const HudItem& hud : hudItems) {
            Gfx::Draw(hdc, hud.icon, iconX[i], hud.y, 40, 40, 0, 0, 38, 38);
            Gfx::DrawOutlinedText(hdc, iconX[i] + 50, hud.y, std::to_wstring(mPlayers[i]->GetPickedCount(hud.type)));
        }
    }
}

void StageScene::RenderResult(HDC hdc) const
{
    Gfx::DrawDarkOverlay(hdc, Config::ScreenWidth, Config::ScreenHeight);

    // game_end 비트맵: 윗줄(0) = LOSE, 아랫줄(65) = WIN
    const int bannerX[PlayerCount] = { 50, 1100 };
    for (int i = 0; i < PlayerCount; ++i) {
        int srcY = (i == mWinner) ? 65 : 0;
        Gfx::Draw(hdc, BitmapID::GameEnd, bannerX[i], 400, 348, 65, 0, srcY, 348, 65);
    }
    Gfx::DrawOutlinedText(hdc, 500, 600, L"다시 시작하려면 R을 누르시오 .");
}
