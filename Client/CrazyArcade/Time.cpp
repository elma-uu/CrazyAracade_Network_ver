#include "Time.h"

LARGE_INTEGER Time::sFrequency = {};
LARGE_INTEGER Time::sPrevCounter = {};
float Time::sDeltaSeconds = 0.0f;

void Time::Initialize()
{
    QueryPerformanceFrequency(&sFrequency);
    QueryPerformanceCounter(&sPrevCounter);
}

void Time::Update()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float delta = static_cast<float>(now.QuadPart - sPrevCounter.QuadPart) / static_cast<float>(sFrequency.QuadPart);
    sPrevCounter = now;
    sDeltaSeconds = std::min(delta, 0.25f);
}
