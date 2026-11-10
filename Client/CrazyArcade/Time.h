#pragma once
#include "Common.h"

class Time {
public:
    static void Initialize();
    static void Update();

    // 이전 프레임으로부터 지난 시간 (창 드래그/디버거 정지 시 폭주하지 않도록 250ms로 제한)
    static float DeltaTime() { return sDeltaSeconds; }
    static float DeltaMs() { return sDeltaSeconds * 1000.0f; }

private:
    static LARGE_INTEGER sFrequency;
    static LARGE_INTEGER sPrevCounter;
    static float sDeltaSeconds;
};
