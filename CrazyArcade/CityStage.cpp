#include "CityStage.h"
#include "AudioManager.h"
#include "Gfx.h"

namespace {
    // 도시 전용 장애물 코드
    constexpr int kBuildingBody = 2;  // 건물이 차지하는 칸 (그림은 건물 기준 칸에서 그림)
    constexpr int kBuilding1 = 3;
    constexpr int kBuilding2 = 4;
    constexpr int kBuilding3 = 5;
    constexpr int kBlinker = 6;       // 신호등: 기차가 오면 꺼진다

    // 블록(1)의 그림: 1 = 가로등, 2 = 쓰레기통
    constexpr int kLamp = 1;
    constexpr int kTrash = 2;

    constexpr int kDropCount = 6;
    const RECT kMapClip = { 320, 0, 1125, 1000 };  // 기차가 맵 밖으로 삐져나오지 않게

    const TileMap::Layout kLayout = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,2,2,0,0,0,0,0,0,0,0,0,0,2,2,0,0,0},
        {0,2,2,2,2,0,0,0,0,0,0,0,0,0,0,2,2,0,0,0},
        {0,2,2,4,2,2,2,0,0,0,0,0,0,2,2,4,2,2,2,0},
        {0,3,2,0,0,3,2,1,1,0,0,1,1,5,2,0,0,5,2,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,6,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,6,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,2,2,0,0,0,0,0,0,0,0,0,0,2,2,0,0,0},
        {0,2,2,2,2,0,0,0,0,0,0,0,0,0,0,2,2,0,0,0},
        {0,2,2,4,2,2,2,0,0,0,0,0,0,2,2,4,2,2,2,0},
        {0,3,2,0,0,3,2,1,1,0,0,1,1,5,2,0,0,5,2,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    };

    // 블록 그림 배치 {row, col, variant}
    struct Placement { int row, col, variant; };
    const Placement kBoxVariants[] = {
        { 4, 7, kLamp },  { 4, 8, kTrash },  { 4, 11, kTrash },  { 4, 12, kLamp },
        { 18, 7, kLamp }, { 18, 8, kTrash }, { 18, 11, kTrash }, { 18, 12, kLamp },
    };
}

void CityStage::BuildMap(TileMap& map)
{
    map.Load(kLayout);
    for (const Placement& p : kBoxVariants) map.At(p.row, p.col).variant = p.variant;
}

POINT CityStage::GetSpawnTile(int playerIndex) const
{
    return playerIndex == 0 ? POINT{ 8, 5 } : POINT{ 11, 14 };
}

void CityStage::AddTime(Player& player, int seconds)
{
    mTimeLeft[player.GetIndex()] += seconds;
}

void CityStage::OnFixedUpdate()
{
    mTrain.FixedUpdate();
    if (mTrain.ShouldDropItems()) DropItems();

    for (auto& item : mFloatingItems) item->FixedUpdate();
    mFloatingItems.erase(std::remove_if(mFloatingItems.begin(), mFloatingItems.end(),
        [](const std::unique_ptr<FloatingItem>& item) { return item->IsDead(); }), mFloatingItems.end());

    for (int i = 0; i < PlayerCount; ++i) {
        Player& player = GetPlayer(i);
        if (mTrain.TryHit(player, TileMap::TileCenter(player.GetRow(), player.GetCol()))) {
            mTimeLeft[i] -= 5;
            player.Hit();
            Audio::PlayEffect("Die.wav");
        }
    }

    mElapsedMs += Config::FixedStepMs;
    if (mElapsedMs >= 1000) {
        mElapsedMs -= 1000;
        for (int& time : mTimeLeft) --time;
    }
    CheckTimeOver();
}

void CityStage::DropItems()
{
    std::vector<POINT> emptyTiles;  // {col, row}
    for (int row = 0; row < TileMap::Size; ++row)
        for (int col = 0; col < TileMap::Size; ++col)
            if (mMap.At(row, col).obstacle == Obstacle::Empty && mMap.At(row, col).item == ItemType::None)
                emptyTiles.push_back({ col, row });

    POINT origin = mTrain.GetDropOrigin();
    for (int i = 0; i < kDropCount && !emptyTiles.empty(); ++i) {
        size_t index = rand() % emptyTiles.size();
        POINT target = emptyTiles[index];
        emptyTiles[index] = emptyTiles.back();
        emptyTiles.pop_back();
        mFloatingItems.push_back(std::make_unique<FloatingItem>(Item::Random(true), origin, target.y, target.x, mMap));
    }
}

void CityStage::CheckTimeOver()
{
    if (mTimeLeft[0] <= 0) EndGame(1);
    else if (mTimeLeft[1] <= 0) EndGame(0);
}

void CityStage::OnPlayerHit(Player& victim, Player&)
{
    mTimeLeft[victim.GetIndex()] -= 2;
    CheckTimeOver();
}

void CityStage::RenderBackground(HDC hdc)
{
    Gfx::DrawOpaque(hdc, BitmapID::CityTileMax, Config::MapOriginX, Config::MapOriginY, 800, 803, 0, 3, 1400, 1400);
}

void CityStage::RenderMap(HDC hdc)
{
    SaveDC(hdc);
    IntersectClipRect(hdc, kMapClip.left, kMapClip.top, kMapClip.right, kMapClip.bottom);

    for (int row = 0; row < TileMap::Size; ++row) {
        for (int col = 0; col < TileMap::Size; ++col) {
            const Tile& tile = mMap.At(row, col);
            POINT c = TileMap::TileCenter(row, col);
            int left = c.x - Config::TileHalf;

            switch (tile.obstacle) {
            case Obstacle::Box:
                if (tile.variant == kLamp)
                    Gfx::Draw(hdc, BitmapID::CityObstacles, left, c.y - 90, 40, 112, 560, 0, 40, 112);
                else if (tile.variant == kTrash)
                    Gfx::Draw(hdc, BitmapID::CityTrashbox, c.x - 25, c.y - 25, 50, 50, 0, 0, 64, 64);
                break;
            case kBuilding1: Gfx::Draw(hdc, BitmapID::CityBuilding1, left, c.y - 120, 80, 140, 0, 0, 68, 149); break;
            case kBuilding2: Gfx::Draw(hdc, BitmapID::CityBuilding2, left, c.y - 140, 80, 160, 0, 0, 68, 166); break;
            case kBuilding3: Gfx::Draw(hdc, BitmapID::CityBuilding3, left, c.y - 120, 80, 140, 0, 0, 68, 109); break;
            case kBlinker: {
                BitmapID light = mTrain.IsRunning() ? BitmapID::CityBlinkerOff : BitmapID::CityBlinkerOn;
                Gfx::Draw(hdc, light, left, c.y - 90, 40, 110, 0, 0, 40, 110);
                break;
            }
            default: break;
            }

            // 기차는 선로 줄(10행)을 그릴 차례에 그려 위아래 오브젝트와 겹침 순서를 맞춘다
            if (row == 10 && col == 0) mTrain.Render(hdc);

            mMap.RenderItemAt(hdc, row, col);
            RenderPlayersAt(hdc, row, col);
        }
    }
    for (const auto& item : mFloatingItems) item->Render(hdc);

    RestoreDC(hdc, -1);
}

void CityStage::RenderHud(HDC hdc)
{
    const int iconX[PlayerCount] = { 100, 1255 };
    for (int i = 0; i < PlayerCount; ++i) {
        Gfx::Draw(hdc, BitmapID::CityClock, iconX[i], 250, 40, 46, 0, 0, 40, 46);
        Gfx::DrawOutlinedText(hdc, iconX[i] + 55, 250, std::to_wstring(mTimeLeft[i]));
    }
}
