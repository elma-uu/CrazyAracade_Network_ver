#pragma once
#include "Common.h"

class GameObject {
public:
    virtual ~GameObject() = default;

    virtual void FixedUpdate() {}
    virtual void Render(HDC hdc) const = 0;

    
    bool IsDead() const { return mDead; }
    void Destroy() { mDead = true; }

private:
    bool mDead = false;
};
