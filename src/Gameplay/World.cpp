/**
 * @file World.cpp
 * @brief Implementation of the game world management.
 */

#include "World.h"
#include "MainMenuScene.h"
#include "../Networking/NetworkManager.h"
#include <spdlog/spdlog.h>

/**
 * @brief Initializes the world and starts in the main menu scene.
 * @param physics Pointer to the physics server.
 */
World::World(PhysicsServer* physics)
    : m_Physics(physics)
{
    m_SceneManager.RequestTransition(new MainMenuScene());
    spdlog::info("World: initialized, starting in MainMenuScene");
}

/**
 * @brief Cleans up physics bodies before destruction.
 */
World::~World() {
    // Clean up bodies in PhysicsServer (for ServerRegistry)
    auto view = m_ServerRegistry.view<PhysicsBodyComponent>();
    for (auto entity : view) {
        auto& body = view.get<PhysicsBodyComponent>(entity);
        if (m_Physics && body.handle.IsValid())
            m_Physics->RemoveBody(body.handle);
    }
}

/**
 * @brief Updates the world, handling scene transitions and fixed/variable updates.
 * @param dt Delta time since last frame.
 * @param net Reference to the network manager.
 * @param renderer Pointer to the renderer.
 */
void World::Update(float dt, NetworkManager& net, Renderer* renderer) {
    SceneContext ctx{m_ServerRegistry, m_ClientRegistry, net, m_SceneManager, renderer};

    /**
     * @brief Apply any pending scene transitions.
     * Transitions are applied at the top of the frame to keep logic consistent.
     */
    m_SceneManager.Flush(ctx);

    /**
     * @brief Handle fixed-rate logic updates.
     */
    m_Accumulator += dt;
    while (m_Accumulator >= FIXED_DT) {
        FixedUpdate(FIXED_DT, net, renderer);
        m_Accumulator -= FIXED_DT;
    }

    /**
     * @brief Handle per-frame updates.
     */
    m_SceneManager.FrameUpdate(ctx, dt);
}

void World::Update(float dt) {
    // Legacy update implementation
}

/**
 * @brief Triggers the current scene's render command.
 * @param renderer Pointer to the renderer.
 * @param net Reference to the network manager.
 */
void World::Render(Renderer* renderer, NetworkManager& net) {
    SceneContext ctx{m_ServerRegistry, m_ClientRegistry, net, m_SceneManager, renderer};
    m_SceneManager.Render(ctx, renderer);
}

/**
 * @brief Updates entity transforms from physics snapshots.
 * @param snapshots Vector of transform snapshots.
 */
void World::ApplySnapshots(const std::vector<TransformSnapshot>& snapshots) {
    for (const auto& snap : snapshots) {
        auto it = m_IDToEntity.find(snap.entityID);
        if (it == m_IDToEntity.end()) continue;

        auto& tf = m_ServerRegistry.get<TransformComponent>(it->second);
        tf.position = glm::vec3(snap.position.GetX(), snap.position.GetY(), snap.position.GetZ());
        
        JPH::Vec3 euler = snap.rotation.GetEulerAngles();
        tf.rotation = glm::vec3(euler.GetX(), euler.GetY(), euler.GetZ());
    }
}

/**
 * @brief Registers an entity for physics synchronization.
 * @param id Physics ID.
 * @param entity EnTT entity.
 */
void World::RegisterPhysicsEntity(uint32_t id, entt::entity entity) {
    m_IDToEntity[id] = entity;
}

/**
 * @brief Unregisters an entity from physics synchronization.
 * @param id Physics ID.
 */
void World::UnregisterPhysicsEntity(uint32_t id) {
    m_IDToEntity.erase(id);
}

/**
 * @brief Performs a fixed-rate logic update via the scene manager.
 * @param dt Fixed delta time.
 * @param net Reference to the network manager.
 * @param renderer Pointer to the renderer.
 */
void World::FixedUpdate(float dt, NetworkManager& net, Renderer* renderer) {
    SceneContext ctx{m_ServerRegistry, m_ClientRegistry, net, m_SceneManager, renderer};
    m_SceneManager.FixedUpdate(ctx, dt);
}
