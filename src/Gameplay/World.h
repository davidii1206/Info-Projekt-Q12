#pragma once
#include <entt/entt.hpp>
#include "Scene.h"
#include "Components.h"
#include "../Core/PhysicsServer.h"

class NetworkManager;
class Renderer;

/**
 * @class World
 * @brief Manages the game state and all active entities.
 */
class World {
public:
    /**
     * @brief Initializes the game world and ECS systems.
     * @param physics The physics server to use.
     */
    explicit World(PhysicsServer* physics);

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

    void Render(Renderer* renderer, NetworkManager& net);

    // Snapshots von PhysicsServer in entt-Components schreiben
    void ApplySnapshots(const std::vector<TransformSnapshot>& snapshots);

    /**
     * @brief Registers an entity for physics snapshot synchronization.
     * @param id The entityID used in PhysicsServer.
     * @param entity The EnTT entity in the server registry.
     */
    void RegisterPhysicsEntity(uint32_t id, entt::entity entity);

    /**
     * @brief Unregisters an entity from physics synchronization.
     */
    void UnregisterPhysicsEntity(uint32_t id);

    // Accessors for debug UI and teammate's rendering system.
    entt::registry& GetServerRegistry() { return m_ServerRegistry; }
    entt::registry& GetClientRegistry() { return m_ClientRegistry; }
    const char*     GetCurrentSceneName() const { return m_SceneManager.GetName(); }
    float           GetAccumulator()      const { return m_Accumulator; }
    PhysicsServer*  GetPhysicsServer()    { return m_Physics; }

private:
    void FixedUpdate(float dt, NetworkManager& net);

    PhysicsServer* m_Physics = nullptr;
    entt::registry m_ServerRegistry; // authoritative — only populated when hosting
    entt::registry m_ClientRegistry; // always active — teammate reads this for rendering
    SceneManager   m_SceneManager;

    float m_Accumulator = 0.f;
    static constexpr float FIXED_DT = 1.f / 20.f; // 20 Hz logic tick for prototype

    uint32_t m_NextEntityID = 1;
    std::unordered_map<uint32_t, entt::entity> m_IDToEntity; // Mapping to ServerRegistry
};
