#pragma once
#include <entt/entt.hpp>
#include "Scene.h"

class NetworkManager;

/**
 * @class World
 * @brief Manages the game state and all active entities.
 * 
 * The World class acts as a container for the ECS (Entity Component System) 
 * registry and orchestrates the updating of game logic with networking support.
 */
class World {
public:
    /**
     * @brief Initializes the game world and ECS systems.
     */
    World();

    /**
     * @brief Cleans up the world and destroys all entities.
     */
    ~World();

    /**
     * @brief Updates all game logic for the current frame.
     * @param dt The time elapsed since the last frame in seconds.
     * @param net The network manager to handle sync.
     */
    void Update(float dt, NetworkManager& net);

    /**
     * @brief Legacy update if needed.
     * @param dt The time elapsed since the last frame in seconds.
     */
    void Update(float dt);

    // Accessors for debug UI and teammate's rendering system.
    entt::registry& GetServerRegistry() { return m_ServerRegistry; }
    entt::registry& GetClientRegistry() { return m_ClientRegistry; }
    const char*     GetCurrentSceneName() const { return m_SceneManager.GetName(); }
    float           GetAccumulator()      const { return m_Accumulator; }

private:
    void FixedUpdate(float dt, NetworkManager& net);

    entt::registry m_ServerRegistry; // authoritative — only populated when hosting
    entt::registry m_ClientRegistry; // always active — teammate reads this for rendering
    SceneManager   m_SceneManager;

    float m_Accumulator = 0.f;
    static constexpr float FIXED_DT = 1.f / 20.f; // 20 Hz logic tick for prototype
};
