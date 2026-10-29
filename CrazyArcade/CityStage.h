#pragma once
#include "StageScene.h"
#include "Train.h"

// 도시: 각자 30초에서 1초씩 줄어들고, 먼저 0이 되면 패배.
// 물풍선에 맞으면 -2초, 기차에 치이면 -5초. 기차가 뿌리는 시계 아이템은 +5초.
class CityStage : public StageScene {
public:
    void AddTime(Player& player, int seconds) override;

protected:
    void BuildMap(TileMap& map) override;
    POINT GetSpawnTile(int playerIndex) const override;
    void OnFixedUpdate() override;

    void OnPlayerHit(Player& victim, Player& attacker) override;

    void RenderBackground(HDC hdc) override;
    void RenderMap(HDC hdc) override;
    void RenderHud(HDC hdc) override;

private:
    void DropItems();
    void CheckTimeOver();

    Train mTrain;
    std::vector<std::unique_ptr<FloatingItem>> mFloatingItems;
    int mTimeLeft[PlayerCount] = { 30, 30 };
    int mElapsedMs = 0;
};
