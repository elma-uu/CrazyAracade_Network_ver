#include "StageScene.h"
#include "SceneManager.h"
#include "AudioManager.h"
#include "GameData.h"
#include "Input.h"
#include "Gfx.h"
#include "GameClient.h"
#include "NetworkManager.h"

namespace {
    const KeyBinding kKeyBindings[2] = {
        { 'W', 'S', 'A', 'D', VK_SPACE },
        { VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_RETURN },
    };
}

StageScene::StageScene()
{
    static_assert(MaxPlayers == MAX_ROOM_PLAYER);
    mOnline = GameClient::IsInGame();
    if (!mOnline) {
        for (int i = 0; i < PlayerCount; ++i)
            mPlayers[i] = std::make_unique<Player>(i, kKeyBindings[i], GameData::playerColor[i], mMap);
        return;
    }
    // 온라인: 서버가 알려 준 참가자만, 자리 번호 그대로 (입력은 서버로 보내므로 키 설정은 쓰지 않음)
    const GameClient::GameStartInfo& start = GameClient::GameStart();
    for (int i = 0; i < MaxPlayers; ++i)
        if (start.players[i].exist)
            mPlayers[i] = std::make_unique<Player>(i, kKeyBindings[0], start.players[i].color, mMap);
}

void StageScene::OnEnter()
{
    Audio::StopMusic();
    Audio::PlayEffect("GameStart.wav");

    BuildMap(mMap);
    if (!mOnline) {
        for (int i = 0; i < PlayerCount; ++i) {
            POINT spawn = GetSpawnTile(i);
            mPlayers[i]->Spawn(spawn.x, spawn.y);
        }
        return;
    }
    // 온라인: 서버가 정한 시작 칸 (맵 네 모서리). 진짜 맵은 뒤이어 오는 S2C_MapInfo가 덮어쓰지만,
    // 그 전까지 블록 위에 캐릭터가 그려지지 않게 시작 칸은 비워 둔다
    const GameClient::GameStartInfo& start = GameClient::GameStart();
    for (int i = 0; i < MaxPlayers; ++i) {
        if (!mPlayers[i]) continue;
        const GameClient::StartPlayerView& sp = start.players[i];
        mMap.At(sp.row, sp.col) = Tile{};
        mPlayers[i]->Spawn(sp.col, sp.row);
    }
}

/* 수정 - 2026-11-11 */
void StageScene::ApplyMapInfo(const S2C_MapInfo& packet)
{
    mNetworkBombs.clear();
    static_assert(TileMap::Size == MAP_TILES);
    for (int row = 0; row < MAP_TILES; ++row) {
        for (int col = 0; col < MAP_TILES; ++col) {
            Tile& tile = mMap.At(row, col);
            tile.obstacle = packet.obstacle[row][col];
            tile.variant = packet.obstacleType[row][col];
            tile.item = ItemType::None; // 숨은 아이템은 서버에서 관리한다.
        }
    }
}

void StageScene::Update(float deltaTime)
{
    for (auto& bomb : mNetworkBombs)
        if (bomb.exploding) bomb.remaining -= deltaTime;
    mNetworkBombs.erase(std::remove_if(mNetworkBombs.begin(), mNetworkBombs.end(),
        [](const NetworkBomb& bomb) { return bomb.exploding && bomb.remaining <= 0; }),
        mNetworkBombs.end());
    if (Input::GetKeyDown(VK_F3)) Player::ToggleFootDebug();   // [디버그] 발밑 중심점 표시

    if (mOnline) {
        // 서버와 끊기면 로그인 화면으로 (끊김 안내는 GameClient가 띄운다)
        if (!NetworkManager::IsConnected()) SceneManager::ChangeScene(SceneType::Login);
        return;
    }
    if (IsGameOver()) {
        if (Input::GetKeyDown('R')) SceneManager::ChangeScene(SceneType::Title);
        return;
    }

    for (auto& player : mPlayers) {
        if (!player) continue;
        player->HandleInput();
        if (player->WantsToPlaceBomb() && CanPlaceBomb(*player)) PlaceBomb(*player);
    }
}

void StageScene::FixedUpdate()
{
    if (mOnline) return;   // 판정은 서버가 한다
    if (IsGameOver()) return;

    for (auto& player : mPlayers) if (player) player->FixedUpdate();
    for (auto& player : mPlayers) if (player) PickUpItem(*player);

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
        if (player && player->GetRow() == row && player->GetCol() == col) player->Render(hdc);
}

void StageScene::Render(HDC hdc)
{
    RenderCommonHud(hdc);
    RenderBackground(hdc);
    for (const auto& bomb : mBombs) bomb->Render(hdc);
    /* 수정 - 2026-11-18 */
    int frame = static_cast<int>((GetTickCount64() / 50) % 4);
    for (const auto& bomb : mNetworkBombs) {
        POINT center = TileMap::TileCenter(bomb.y, bomb.x);
        if (!bomb.exploding) {
            Gfx::Draw(hdc, BitmapID::BombSet,
                center.x - Config::TileHalf, center.y - Config::TileHalf,
                Config::TileSize, Config::TileSize, frame * 56, 0, 54, 54);
            continue;
        }
        // 11/20: 원본 Bomb::Render의 물줄기 이미지 규격을 사용한다.
        int flameFrame = static_cast<int>(bomb.remaining * 1000 / 50) % 3;
        auto drawFlame = [&](BitmapID image, int row, int col) {
            POINT pos = TileMap::TileCenter(row, col);
            Gfx::Draw(hdc, image, pos.x - Config::TileHalf, pos.y - Config::TileHalf,
                Config::TileSize, Config::TileSize, flameFrame * 52, 0, 52, 52);
        };
        drawFlame(BitmapID::BombCenter, bomb.y, bomb.x);
        for (int i = 1; i <= bomb.up; ++i) drawFlame(BitmapID::BombUp, bomb.y - i, bomb.x);
        for (int i = 1; i <= bomb.down; ++i) drawFlame(BitmapID::BombDown, bomb.y + i, bomb.x);
        for (int i = 1; i <= bomb.left; ++i) drawFlame(BitmapID::BombLeft, bomb.y, bomb.x - i);
        for (int i = 1; i <= bomb.right; ++i) drawFlame(BitmapID::BombRight, bomb.y, bomb.x + i);
    }

    RenderMap(hdc);
    if (mOnline) {
        RenderOnlineNames(hdc);
        return;   // 스테이지 HUD(사과·시간·하트)는 S2C_StageState를 붙인 뒤에
    }
    RenderHud(hdc);
    if (IsGameOver()) RenderResult(hdc);
}

void StageScene::RenderOnlineNames(HDC hdc) const
{
    // 누가 누구인지 알 수 있게 캐릭터 머리 위에 닉네임, 내 캐릭터는 노란색
    const GameClient::GameStartInfo& start = GameClient::GameStart();
    for (int i = 0; i < MaxPlayers; ++i) {
        if (!mPlayers[i]) continue;
        POINT c = TileMap::TileCenter(mPlayers[i]->GetRow(), mPlayers[i]->GetCol());
        COLORREF color = (i == start.mySlot) ? RGB(255, 230, 0) : RGB(255, 255, 255);
        Gfx::DrawSmallText(hdc, c.x - 30, c.y - 62, start.players[i].name, color);
    }
}

void StageScene::RenderCommonHud(HDC hdc) const
{
    Gfx::DrawOpaque(hdc, BitmapID::GameUI, 0, 0, 1450, 900, 0, 0, 1040, 780);
    if (mOnline) return;   // 2인용 초상화·아이템 칸은 한 컴퓨터 모드에서만

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

/* 수정 - 2026-11-18 */
bool StageScene::OnAttack(const S2C_Attack& packet)
{
    if (packet.size != sizeof(packet) || packet.type != S2C_ATTACK
        || packet.bombId < 0 || packet.ownerId < 0 || packet.reach == 0
        || !TileMap::InBounds(packet.y, packet.x)) return false;
    for (const auto& bomb : mNetworkBombs)
        if (bomb.bombId == packet.bombId) return false;
    mNetworkBombs.emplace_back(packet);
    mMap.At(packet.y, packet.x).obstacle = Obstacle::Bomb;
    Audio::PlayEffect("Ballon.wav");
    return true;
}

/* 수정 - 2026-11-20 */
bool StageScene::OnExplode(const S2C_Explode& packet)
{
    if (packet.size != sizeof(packet) || packet.type != S2C_EXPLODE) return false;
    for (auto& bomb : mNetworkBombs) {
        if (bomb.bombId != packet.bombId) continue;
        if (bomb.exploding || packet.reachLeft > bomb.x || packet.reachRight >= MAP_TILES - bomb.x
            || packet.reachUp > bomb.y || packet.reachDown >= MAP_TILES - bomb.y) return false;
        bomb.exploding = true;
        bomb.up = packet.reachUp;
        bomb.down = packet.reachDown;
        bomb.left = packet.reachLeft;
        bomb.right = packet.reachRight;
        mMap.At(bomb.y, bomb.x).obstacle = Obstacle::Empty;
        Audio::PlayEffect("Bubble.wav");
        return true;
    }
    return false;
}

/* 11/21 */
bool StageScene::OnBlockBreak(const S2C_BlockBreak& packet)
{
    if (packet.size != sizeof(packet) || packet.type != S2C_BLOCK_BREAK
        || !TileMap::InBounds(packet.y, packet.x) || packet.item > ITEM_CLOCK) return false;
    Tile& tile = mMap.At(packet.y, packet.x);
    tile.obstacle = Obstacle::Empty;
    tile.variant = 0;
    tile.item = static_cast<ItemType>(packet.item);
    return true;
}
