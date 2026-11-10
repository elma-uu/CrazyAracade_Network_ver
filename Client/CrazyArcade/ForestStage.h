#pragma once
#include "StageScene.h"

// 숲: 사과나무를 물줄기로 맞혀 사과를 얻고, 자기 바구니에 10개 먼저 넣으면 승리.
// 들고 있는 사과 수만큼 놓을 수 있는 물풍선이 줄고, 맞으면 사과를 상대에게 빼앗긴다.
class ForestStage : public StageScene {
protected:
    void BuildMap(TileMap& map) override;
    POINT GetSpawnTile(int playerIndex) const override;
    void OnFixedUpdate() override;

    bool CanPlaceBomb(const Player& player) const override;
    void OnPlayerHit(Player& victim, Player& attacker) override;
    void OnFlameBlocked(Tile& tile, Player& owner) override;

    void RenderBackground(HDC hdc) override;
    void RenderMap(HDC hdc) override;
    void RenderHud(HDC hdc) override;

private:
    int mBasketScore[PlayerCount] = {};
};
