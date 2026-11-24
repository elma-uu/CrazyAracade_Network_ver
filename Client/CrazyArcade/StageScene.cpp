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
    // 온라인은 서버가 알려 준 참가자만
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
    // MapInfo가 오기 전까지 시작 칸은 비워 둔다
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
    if (Input::GetKeyDown(VK_F3)) Player::ToggleFootDebug();   // 발밑 중심점 보기

    if (mOnline) {
        // 끊기면 로그인 화면으로
        if (!NetworkManager::IsConnected()) {
            SceneManager::ChangeScene(SceneType::Login);
            return;
        }
        UpdateOnlineInput();

        // 서버가 보낸 이동 반영
        for (int i = 0; i < MaxPlayers; ++i) {
            if (!mPlayers[i]) continue;
            GameClient::MoveView m;
            if (GameClient::TakeMove(i, m))
                mPlayers[i]->SetNetMove(m.x, m.y, m.destX, m.destY, m.dir, m.speed);
            mPlayers[i]->UpdateNetMove(deltaTime);
        }
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

// 방향이 바뀔 때만 C2S_Input을 보낸다
void StageScene::UpdateOnlineInput()
{
    // 위, 아래, 왼쪽, 오른쪽
    static const int keys1[4] = { VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT };
    static const int keys2[4] = { 'W', 'S', 'A', 'D' };

    // 이번 프레임에 새로 누른 방향을 맨 앞으로
    for (int d = 0; d < 4; d++) {
        if (Input::GetKeyDown(keys1[d]) || Input::GetKeyDown(keys2[d])) {
            int pos = 0;
            for (int k = 0; k < 4; k++) if (mInputOrder[k] == d) pos = k;
            for (int k = pos; k > 0; k--) mInputOrder[k] = mInputOrder[k - 1];
            mInputOrder[0] = d;
        }
    }

    // 누르고 있는 것 중 가장 최근 것
    INPUT_DIR dir = IN_NONE;
    for (int k = 0; k < 4; k++) {
        int d = mInputOrder[k];
        if (Input::GetKey(keys1[d]) || Input::GetKey(keys2[d])) {
            dir = (INPUT_DIR)(IN_UP + d);
            break;
        }
    }

    if (dir != mSentDir) {
        NetworkManager::SendInput(dir);
        mSentDir = dir;
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
        RenderOnlineHud(hdc);
        return;
    }
    RenderHud(hdc);
    if (IsGameOver()) RenderResult(hdc);
}

RECT StageScene::OnlineCardRect(int slot)
{
    const int w = 256, h = 196;
    int x = (slot % 2 == 0) ? 26 : 1193;   // 맵 양옆 빈 곳의 가운데
    int y = (slot < 2) ? 40 : Config::ScreenHeight - 40 - h;
    return { x, y, x + w, y + h };
}

RECT StageScene::DrawOnlineCard(HDC hdc, int slot) const
{
    const GameClient::GameStartInfo& start = GameClient::GameStart();
    const bool me = (slot == start.mySlot);
    RECT card = OnlineCardRect(slot);

    Gfx::FillAlpha(hdc, card, RGB(6, 32, 74), 170);
    Gfx::DrawRoundFrame(hdc, card, 10, me ? RGB(255, 210, 40) : RGB(90, 170, 240), 2);

    // 위쪽: 초상화, 이름, 나/P번호
    if (mPlayers[slot]) mPlayers[slot]->RenderPortrait(hdc, card.left + 12, card.top + 10);
    Gfx::DrawPixelText(hdc, { card.left + 100, card.top + 10, card.right - 12, card.top + 36 },
        start.players[slot].name, me ? RGB(255, 230, 0) : RGB(255, 255, 255), Gfx::Align::Left, 20);
    Gfx::DrawPixelText(hdc, { card.left + 100, card.top + 38, card.right - 12, card.top + 60 },
        me ? L"나" : L"P" + std::to_wstring(slot + 1), me ? RGB(255, 230, 0) : RGB(150, 180, 220), Gfx::Align::Left, 16);

    // 구분선
    RECT line = { card.left + 12, card.top + 68, card.right - 12, card.top + 70 };
    Gfx::FillAlpha(hdc, line, RGB(90, 170, 240), 150);

    return { card.left, card.top + 72, card.right, card.bottom - 6 };
}

void StageScene::DrawOnlineCardOut(HDC hdc, int slot) const
{
    if (!GameClient::HasStageState() || GameClient::Stage(slot).alive) return;
    RECT card = OnlineCardRect(slot);
    Gfx::FillAlpha(hdc, card, RGB(0, 0, 0), 150);
    // 위쪽 오른편에 뱃지 (내용 줄과 안 겹치게)
    RECT badge = { card.right - 84, card.top + 16, card.right - 12, card.top + 52 };
    Gfx::FillAlpha(hdc, badge, RGB(170, 30, 30), 230);
    Gfx::DrawRoundFrame(hdc, badge, 6, RGB(255, 120, 120), 2);
    Gfx::DrawPixelText(hdc, badge, L"탈락", RGB(255, 255, 255), Gfx::Align::Center, 24, RGB(90, 0, 0), 1);
}

void StageScene::DrawOnlineStageInfo(HDC hdc, const wchar_t* title, const wchar_t* sub,
    std::initializer_list<const wchar_t*> rules) const
{
    // 왼쪽 가운데: 스테이지 이름과 규칙
    RECT info = { 26, 300, 282, 600 };
    Gfx::FillAlpha(hdc, info, RGB(6, 32, 74), 120);
    Gfx::DrawRoundFrame(hdc, info, 10, RGB(60, 130, 210), 2);
    Gfx::DrawPixelText(hdc, { info.left, info.top + 16, info.right, info.top + 48 }, title, RGB(255, 230, 0), Gfx::Align::Center, 32);
    Gfx::DrawPixelText(hdc, { info.left, info.top + 54, info.right, info.top + 74 }, sub, RGB(150, 200, 255), Gfx::Align::Center, 16);
    RECT line = { info.left + 16, info.top + 88, info.right - 16, info.top + 90 };
    Gfx::FillAlpha(hdc, line, RGB(90, 170, 240), 150);
    int y = info.top + 106;
    for (const wchar_t* rule : rules) {
        Gfx::DrawPixelText(hdc, { info.left + 18, y, info.right - 12, y + 22 }, rule, RGB(255, 255, 255), Gfx::Align::Left, 16);
        y += 34;
    }

    // 오른쪽 가운데: 남은 인원
    const GameClient::GameStartInfo& start = GameClient::GameStart();
    int total = 0, alive = 0;
    for (int i = 0; i < MaxPlayers; ++i) {
        if (!start.players[i].exist) continue;
        ++total;
        if (!GameClient::HasStageState() || GameClient::Stage(i).alive) ++alive;
    }
    RECT count = { 1193, 380, 1449, 520 };
    Gfx::FillAlpha(hdc, count, RGB(6, 32, 74), 120);
    Gfx::DrawRoundFrame(hdc, count, 10, RGB(60, 130, 210), 2);
    Gfx::DrawPixelText(hdc, { count.left, count.top + 18, count.right, count.top + 42 }, L"생존", RGB(150, 200, 255), Gfx::Align::Center, 24);
    Gfx::DrawPixelText(hdc, { count.left, count.top + 60, count.right, count.top + 108 },
        std::to_wstring(alive) + L" / " + std::to_wstring(total), RGB(255, 255, 255), Gfx::Align::Center, 48, RGB(8, 36, 90), 2);
}

void StageScene::RenderCommonHud(HDC hdc) const
{
    if (mOnline) {
        // 온라인에서는 2인용 패널(TIMER, ITEM)을 안 쓰므로 배경색과 맵 테두리만 그린다
        Gfx::DrawOpaque(hdc, BitmapID::GameUI, 0, 0, Config::ScreenWidth, Config::ScreenHeight, 825, 0, 4, 780);
        Gfx::DrawOpaque(hdc, BitmapID::GameUI, 296, 0, 854, 900, 212, 0, 613, 780);
        // 좌우 카드들을 큰 박스로 한 번 감싼다
        const RECT sides[2] = { { 12, 24, 296, 876 }, { 1179, 24, 1463, 876 } };
        for (const RECT& side : sides) {
            Gfx::FillAlpha(hdc, side, RGB(4, 40, 110), 140);
            Gfx::DrawRoundFrame(hdc, side, 14, RGB(70, 150, 230), 3);
            RECT inner = { side.left + 5, side.top + 5, side.right - 5, side.bottom - 5 };
            Gfx::DrawRoundFrame(hdc, inner, 10, RGB(20, 90, 180), 1);
        }
        return;
    }
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
