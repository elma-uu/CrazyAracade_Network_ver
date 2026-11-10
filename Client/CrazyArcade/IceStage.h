#pragma once
#include "StageScene.h"

// 얼음: 하트 3개, 물풍선에 맞을 때마다 하나씩 잃고 0이 되면 패배
class IceStage : public StageScene {
protected:
    void BuildMap(TileMap& map) override;
    POINT GetSpawnTile(int playerIndex) const override;

    void OnPlayerHit(Player& victim, Player& attacker) override;

    void RenderBackground(HDC hdc) override;
    void RenderMap(HDC hdc) override;
    void RenderHud(HDC hdc) override;

private:
    int mHearts[PlayerCount] = { 3, 3 };
};
