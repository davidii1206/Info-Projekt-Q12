#pragma once
#include <entt/entt.hpp>
#include "Scene.h"

class NetworkManager;

class World {
public:
    World();

    // Called every render frame. Internally runs fixed-tick logic.
    void Update(float dt, NetworkManager& net);

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
