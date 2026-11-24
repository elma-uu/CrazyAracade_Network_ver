#include "ForestStage.h"
#include "Gfx.h"
#include "GameClient.h"

namespace {
    // 숲 전용 장애물 코드
    constexpr int kAppleTree = 2;
    constexpr int kBasket = 3;

    constexpr int kWinScore = 10;
    // 바구니 위치 {col, row}
    const POINT kBaskets[2] = { { 8, 19 }, { 11, 19 } };

    const TileMap::Layout kLayout = {
        {0,0,1,1,0,1,0,0,0,0,0,0,0,0,1,0,1,1,0,0},
        {0,0,0,1,0,0,0,0,0,2,2,0,0,0,0,0,1,0,0,0},
        {1,0,0,0,0,0,1,0,0,2,2,0,0,1,0,0,0,0,0,1},
        {0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0,0,0},
        {0,0,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1,1,0,0},
        {0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0},
        {0,1,0,0,1,0,0,0,0,1,1,0,0,0,0,1,0,0,1,0},
        {1,1,1,0,1,1,1,0,0,1,1,0,0,1,1,1,0,1,1,1},
        {1,1,1,1,1,1,0,0,1,1,1,1,0,0,1,1,1,1,1,1},
        {1,1,1,1,1,1,0,0,1,1,1,1,0,0,1,1,1,1,1,1},
        {1,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,0,1},
        {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0},
        {0,0,1,0,0,1,0,0,0,1,1,0,0,0,1,0,0,1,0,0},
        {0,1,1,1,0,1,1,0,0,0,0,0,0,1,1,0,1,1,1,0},
        {0,0,0,1,1,1,1,0,1,1,1,1,0,1,1,1,1,0,0,0},
        {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
        {0,0,0,0,1,0,0,1,1,1,1,1,1,0,0,1,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0},
        {1,1,0,0,0,0,0,0,3,1,1,3,0,0,0,0,0,0,1,1},
    };

    bool IsNextToBasket(const Player& player, POINT basket)
    {
        int col = player.GetCol(), row = player.GetRow();
        return (col == basket.x && row == basket.y - 1)
            || (col == basket.x - 1 && row == basket.y)
            || (col == basket.x + 1 && row == basket.y);
    }
}

void ForestStage::BuildMap(TileMap& map)
{
    map.Load(kLayout);
    for (int row = 0; row < TileMap::Size; ++row) {
        for (int col = 0; col < TileMap::Size; ++col) {
            Tile& tile = map.At(row, col);
            if (tile.obstacle != Obstacle::Box) continue;
            tile.variant = rand() % 5;
            if (rand() % 10 > 5) tile.item = Item::Random();
        }
    }
}

POINT ForestStage::GetSpawnTile(int playerIndex) const
{
    return playerIndex == 0 ? POINT{ 8, 18 } : POINT{ 11, 18 };
}

void ForestStage::OnFixedUpdate()
{
    for (int i = 0; i < PlayerCount; ++i) {
        Player& player = GetPlayer(i);
        if (player.GetApples() > 0 && IsNextToBasket(player, kBaskets[i])) {
            mBasketScore[i] += player.GetApples();
            player.SetApples(0);
        }
    }
    for (int i = 0; i < PlayerCount; ++i) {
        if (mBasketScore[i] >= kWinScore) { EndGame(i); return; }
    }
}

bool ForestStage::CanPlaceBomb(const Player& player) const
{
    return CountBombs(player) < player.GetMaxBombs() - player.GetApples();
}

void ForestStage::OnPlayerHit(Player& victim, Player& attacker)
{
    attacker.SetApples(attacker.GetApples() + victim.GetApples());
    victim.SetApples(0);
}

void ForestStage::OnFlameBlocked(Tile& tile, Player& owner)
{
    if (tile.obstacle == kAppleTree) owner.SetApples(owner.GetApples() + 1);
}

void ForestStage::RenderBackground(HDC hdc)
{
    Gfx::DrawOpaque(hdc, BitmapID::ForestTileMax, Config::MapOriginX, Config::MapOriginY, 800, 800, 0, 0, 600, 520);
}

void ForestStage::RenderMap(HDC hdc)
{
    static const BitmapID kBoxes[5] = {
        BitmapID::ForestObstacle1, BitmapID::ForestObstacle2, BitmapID::ForestObstacle3,
        BitmapID::ForestObstacle4, BitmapID::ForestObstacle5,
    };

    for (int row = 0; row < TileMap::Size; ++row) {
        for (int col = 0; col < TileMap::Size; ++col) {
            const Tile& tile = mMap.At(row, col);
            POINT c = TileMap::TileCenter(row, col);

            // 큰 사과나무는 2x2 칸을 덮으므로 (2, 11)에 도달했을 때 한 번 그린다
            if (row == 2 && col == 11) {
                POINT anchor = TileMap::TileCenter(1, 8);
                Gfx::Draw(hdc, BitmapID::ForestTree, anchor.x - 10, anchor.y - 60, 140, 120, 0, 0, 100, 80);
            }

            mMap.RenderItemAt(hdc, row, col);
            RenderPlayersAt(hdc, row, col);

            if (tile.obstacle == Obstacle::Box)
                Gfx::Draw(hdc, kBoxes[tile.variant], c.x - 25, c.y - 25, 50, 50, 0, 0, 68, 67);
            else if (tile.obstacle == kBasket)
                Gfx::Draw(hdc, BitmapID::ForestAppleBox, c.x - 30, c.y - 50, 60, 70, 0, 0, 64, 64);
        }
    }
}

// 온라인 숲 HUD: 자리마다 들고 있는 사과, 사과상자
// 카드는 시작 모서리 쪽에 (StageScene::OnlineCardRect)
void ForestStage::RenderOnlineHud(HDC hdc)
{
    const GameClient::GameStartInfo& start = GameClient::GameStart();

    DrawOnlineStageInfo(hdc, L"숲", L"사과 쟁탈전", {
        L"사과나무를 터뜨려 줍기",
        L"내 상자 옆에서 저장",
        L"맞으면 든 사과를 뺏김",
        L"사과 없이 맞으면 상자 -1",
        L"상자가 비었는데 맞으면 탈락" });

    for (int i = 0; i < MaxPlayers; i++) {
        if (!start.players[i].exist) continue;

        RECT body = DrawOnlineCard(hdc, i);
        const GameClient::StageView& s = GameClient::Stage(i);

        // 두 줄: 사과 / 사과상자
        int rowH = (body.bottom - body.top) / 2;
        for (int row = 0; row < 2; row++) {
            int top = body.top + row * rowH;
            if (row == 0)
                Gfx::Draw(hdc, BitmapID::ForestApple, body.left + 16, top + (rowH - 36) / 2, 36, 36, 0, 0, 40, 40);
            else
                Gfx::Draw(hdc, BitmapID::ForestAppleBox, body.left + 16, top + (rowH - 36) / 2, 36, 36, 0, 0, 64, 64);

            RECT text = { body.left + 64, top, body.right - 18, top + rowH };
            Gfx::DrawPixelText(hdc, text, row == 0 ? L"사과" : L"사과상자", RGB(255, 255, 255), Gfx::Align::Left, 24);
            int count = (row == 0) ? s.apple : s.appleBox;
            Gfx::DrawPixelText(hdc, text, std::to_wstring(count), RGB(255, 230, 0), Gfx::Align::Right, 24);
        }

        DrawOnlineCardOut(hdc, i);
    }
}

void ForestStage::RenderHud(HDC hdc)
{
    const int iconX[PlayerCount] = { 50, 1200 };
    const int textX[PlayerCount] = { 125, 1280 };

    for (int i = 0; i < PlayerCount; ++i) {
        Gfx::Draw(hdc, BitmapID::ForestApple, iconX[i], 220, 64, 64, 0, 0, 40, 40);
        Gfx::Draw(hdc, BitmapID::ForestAppleBox, iconX[i], 260, 64, 64, 0, 0, 64, 64);

        Gfx::DrawOutlinedText(hdc, textX[i], 220, L" : ");
        Gfx::DrawOutlinedText(hdc, textX[i] + 90, 220, std::to_wstring(GetPlayer(i).GetApples()));
        Gfx::DrawOutlinedText(hdc, textX[i], 270, L" : ");
        Gfx::DrawOutlinedText(hdc, textX[i] + 90, 270, std::to_wstring(mBasketScore[i]));
    }
}
