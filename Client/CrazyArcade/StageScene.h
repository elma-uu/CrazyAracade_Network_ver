#pragma once
#include "Scene.h"
#include "TileMap.h"
#include "../../Common/protocol.h"
#include "Player.h"
#include "Bomb.h"

// 세 스테이지가 공유하는 대전 규칙: 2인 이동/물풍선/아이템/승패/공통 HUD.
// 스테이지별 맵, 피격 규칙, 승리 조건, 그리기는 파생 클래스가 채운다.
// 온라인 모드에서는 서버가 판정하고 클라는 그리기만 한다
class StageScene : public Scene {
public:
    StageScene();
    bool OnBlockBreak(const S2C_BlockBreak& packet);
    bool OnExplode(const S2C_Explode& packet);
    bool OnAttack(const S2C_Attack& packet);
    void ApplyMapInfo(const S2C_MapInfo& packet);

    void OnEnter() override;
    void Update(float deltaTime) override;
    void FixedUpdate() override;
    void Render(HDC hdc) override;

    // ClockItem 효과. 시간제가 아닌 스테이지는 무시
    virtual void AddTime(Player& player, int seconds) {}

protected:
    // 맵 구성 (OnEnter에서 호출)
    virtual void BuildMap(TileMap& map) = 0;
    // {col, row}
    virtual POINT GetSpawnTile(int playerIndex) const = 0;
    // 스테이지 전용 로직 (기차, 타이머, 사과 바구니 등)
    virtual void OnFixedUpdate() {}

    virtual bool CanPlaceBomb(const Player& player) const;
    // 상대 물풍선에 맞았을 때 스테이지별 페널티
    virtual void OnPlayerHit(Player& victim, Player& attacker) = 0;
    // 물줄기가 부서지지 않는 장애물에 막혔을 때
    virtual void OnFlameBlocked(Tile& tile, Player& owner) {}

    virtual void RenderBackground(HDC hdc) = 0;
    // 타일/장애물/플레이어를 행 순서로 그려 앞뒤가 맞게 겹치도록 한다
    virtual void RenderMap(HDC hdc) = 0;
    virtual void RenderHud(HDC hdc) {}
    // 온라인 HUD (S2C_StageState 값으로)
    virtual void RenderOnlineHud(HDC hdc) {}

    void EndGame(int winnerIndex);
    bool IsGameOver() const { return mWinner >= 0; }

    Player& GetPlayer(int index) { return *mPlayers[index]; }
    Player& GetOpponent(const Player& player) { return *mPlayers[1 - player.GetIndex()]; }
    int CountBombs(const Player& owner) const;

    // RenderMap 도중 (row, col)에 서 있는 플레이어를 그린다
    void RenderPlayersAt(HDC hdc, int row, int col) const;

    bool IsOnline() const { return mOnline; }

    static constexpr int PlayerCount = 2;   // 한 컴퓨터 2인 모드
    static constexpr int MaxPlayers = 4;    // 온라인 최대 인원
    TileMap mMap;
    std::unique_ptr<Player> mPlayers[MaxPlayers];

private:
    bool mOnline = false;

    // 온라인 이동 입력
    void UpdateOnlineInput();
    int mInputOrder[4] = { 0, 1, 2, 3 };   // 최근에 누른 방향 순서
    INPUT_DIR mSentDir = IN_NONE;          // 마지막으로 서버에 보낸 방향

    void PlaceBomb(Player& player);
    void UpdateBombs();
    void PickUpItem(Player& player);
    void RenderCommonHud(HDC hdc) const;
    void RenderOnlineNames(HDC hdc) const;
    void RenderResult(HDC hdc) const;

    std::vector<std::unique_ptr<Bomb>> mBombs;
    struct NetworkBomb : S2C_Attack {
        explicit NetworkBomb(const S2C_Attack& packet) : S2C_Attack(packet) {}
        bool exploding = false;
        float remaining = 0.5f;
        unsigned char up = 0, down = 0, left = 0, right = 0;
    };
    std::vector<NetworkBomb> mNetworkBombs;
    int mWinner = -1;
};
