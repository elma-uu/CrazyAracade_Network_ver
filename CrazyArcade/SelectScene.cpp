#include "SelectScene.h"
#include "SceneManager.h"
#include "GameData.h"
#include "Input.h"
#include "Gfx.h"

namespace {
    struct ColorKeys { int prev, next; };
    const ColorKeys kColorKeys[2] = { { 'A', 'D' }, { VK_LEFT, VK_RIGHT } };

    // 색상 칸 체크 표시 위치
    constexpr int kCheckBaseX = 270;
    constexpr int kCheckSpacing = 155;
    const int kCheckY[2] = { 670, 640 };
    const BitmapID kCheckIcons[2] = { BitmapID::P1Check, BitmapID::P2Check };

    bool StagePressed(int digit)
    {
        return Input::GetKeyDown('0' + digit) || Input::GetKeyDown(VK_NUMPAD0 + digit);
    }
}

void SelectScene::OnEnter()
{
    mColor[0] = mColor[1] = 0;
}

void SelectScene::Update(float)
{
    for (int i = 0; i < 2; ++i) {
        if (Input::GetKeyDown(kColorKeys[i].prev) && mColor[i] > 0) --mColor[i];
        if (Input::GetKeyDown(kColorKeys[i].next) && mColor[i] < GameData::ColorCount - 1) ++mColor[i];
    }

    const SceneType stages[] = { SceneType::Forest, SceneType::City, SceneType::Ice };
    for (int digit = 1; digit <= 3; ++digit) {
        if (!StagePressed(digit)) continue;
        GameData::playerColor[0] = mColor[0];
        GameData::playerColor[1] = mColor[1];
        SceneManager::ChangeScene(stages[digit - 1]);
        return;
    }
}

void SelectScene::Render(HDC hdc)
{
    Gfx::DrawOpaque(hdc, BitmapID::SelectStage, 0, 0, Config::ScreenWidth, Config::ScreenHeight, 0, 0, 1500, 900);
    for (int i = 0; i < 2; ++i)
        Gfx::Draw(hdc, kCheckIcons[i], kCheckBaseX + mColor[i] * kCheckSpacing, kCheckY[i], 100, 50, 0, 0, 224, 150);
}
