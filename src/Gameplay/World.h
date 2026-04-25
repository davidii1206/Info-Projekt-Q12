/**
 * @file World.h
 * @brief Header for the game world management.
 */

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
 * 
 * The World class is responsible for updating game logic, managing 
 * the scene life cycle, and handling physics synchronization.
 */
class World {
public:
    /**
     * @brief Initializes the game world and ECS systems.
     * @param physics Pointer to the physics server instance.
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
     * @param renderer Pointer to the renderer instance.
     */
    void Update(float dt, NetworkManager& net, Renderer* renderer);

    /**
     * @brief Legacy update function.
     * @param dt The time elapsed since the last frame in seconds.
     */
    void Update(float dt);

    /**
     * @brief Renders the current world state.
     * @param renderer Pointer to the renderer instance.
     * @param net Reference to the network manager.
     */
    void Render(Renderer* renderer, NetworkManager& net);

    /**
     * @brief Applies physics snapshots to EnTT components.
     * @param snapshots Vector of transform snapshots from the physics server.
     */
    void ApplySnapshots(const std::vector<TransformSnapshot>& snapshots);

    /**
     * @brief Registers an entity for physics snapshot synchronization.
     * @param id The entityID used in PhysicsServer.
     * @param entity The EnTT entity in the server registry.
     */
    void RegisterPhysicsEntity(uint32_t id, entt::entity entity);

    /**
     * @brief Unregisters an entity from physics synchronization.
     * @param id The entityID used in PhysicsServer.
     */
    void UnregisterPhysicsEntity(uint32_t id);

    /**
     * @brief Gets the authoritative server registry.
     * @return Reference to the server entt::registry.
     */
    entt::registry& GetServerRegistry() { return m_ServerRegistry; }

    /**
     * @brief Gets the client registry used for rendering.
     * @return Reference to the client entt::registry.
     */
    entt::registry& GetClientRegistry() { return m_ClientRegistry; }

    /**
     * @brief Gets the name of the currently active scene.
     * @return The scene name as a string.
     */
    const char*     GetCurrentSceneName() const { return m_SceneManager.GetName(); }

    /**
     * @brief Gets the current fixed update accumulator.
     * @return The accumulator value.
     */
    float           GetAccumulator()      const { return m_Accumulator; }

    /**
     * @brief Gets the next available physics ID for synchronization.
     * @return The next physics ID.
     */
    uint32_t        GetNextPhysicsID()    { return m_NextEntityID++; }

    /**
     * @brief Gets the physics server instance.
     * @return Pointer to the PhysicsServer.
     */
    PhysicsServer*  GetPhysicsServer()    { return m_Physics; }

private:
    /**
     * @brief Performs a fixed-rate logic update.
     * @param dt The fixed delta time.
     * @param net Reference to the network manager.
     * @param renderer Pointer to the renderer instance.
     */
    void FixedUpdate(float dt, NetworkManager& net, Renderer* renderer);

    /// Pointer to the physics server.
    PhysicsServer* m_Physics = nullptr;
    /// Authoritative registry — only populated when hosting.
    entt::registry m_ServerRegistry;
    /// Always active registry — used for local rendering and client state.
    entt::registry m_ClientRegistry;
    /// Manager for scene transitions and updates.
    SceneManager   m_SceneManager;

    /// Accumulator for fixed-rate updates.
    float m_Accumulator = 0.f;
    /// Fixed delta time for logic ticks (60 Hz).
    static constexpr float FIXED_DT = 1.f / 60.f;

    /// ID to assign to the next registered entity.
    uint32_t m_NextEntityID = 1;
    /// Mapping from physics ID to EnTT entity.
    std::unordered_map<uint32_t, entt::entity> m_IDToEntity;
};
