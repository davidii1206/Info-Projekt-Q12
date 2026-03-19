#pragma once
#include <SDL3/SDL.h>

class Timer {
public:
    Timer() {
        m_LastTime = SDL_GetTicks();
    }

    void Update() {
        uint64_t currentTime = SDL_GetTicks();
        m_DeltaTime = (currentTime - m_LastTime) / 1000.0f;
        m_LastTime = currentTime;
    }

    float GetDeltaTime() const { return m_DeltaTime; }
    float GetFPS() const { return 1.0f / m_DeltaTime; }

private:
    uint64_t m_LastTime;
    float m_DeltaTime = 0.0f;
};