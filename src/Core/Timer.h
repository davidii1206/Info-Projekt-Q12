/**
 * @file Timer.h
 * @brief Simple frame timer for calculating delta time and FPS.
 */

#pragma once
#include <SDL3/SDL.h>

/**
 * @class Timer
 * @brief Tracks time between frames for delta-time calculations.
 */
class Timer {
public:
    /**
     * @brief Initializes the timer with the current SDL ticks.
     */
    Timer() {
        m_LastTime = SDL_GetTicks();
    }

    /**
     * @brief Updates the timer, calculating delta time since the last update.
     * 
     * This should be called once per frame in the main loop.
     */
    void Update() {
        uint64_t currentTime = SDL_GetTicks();
        m_DeltaTime = (currentTime - m_LastTime) / 1000.0f;
        m_LastTime = currentTime;
    }

    /**
     * @brief Returns the time elapsed between the last two Update() calls.
     * @return float Delta time in seconds.
     */
    float GetDeltaTime() const { return m_DeltaTime; }

    /**
     * @brief Calculates the approximate frames per second.
     * @return float Current FPS.
     */
    float GetFPS() const { return 1.0f / m_DeltaTime; }

private:
    uint64_t m_LastTime; ///< Time of the last update in milliseconds.
    float m_DeltaTime = 0.0f; ///< Delta time in seconds.
};
