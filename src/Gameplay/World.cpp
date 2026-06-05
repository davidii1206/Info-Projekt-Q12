/**
 * @file World.cpp
 * @brief Implementation of the game world management.
 */

#include "World.h"
#include "MainMenuScene.h"
#include "../Networking/NetworkManager.h"
#include <spdlog/spdlog.h>
#include <glm/gtc/quaternion.hpp>

/**
 * @brief Initializes the world and starts in the main menu scene.
 * @param physics Pointer to the physics server.
 */
World::World(PhysicsServer* physics)
    : m_Physics(physics)
{
    // Start physics IDs high to avoid conflict with early network IDs
    m_NextEntityID = 10000;
    
    m_SceneManager.RequestTransition(new MainMenuScene());
    spdlog::info("World: initialized, starting in MainMenuScene");
}

/**
 * @brief Cleans up the world and destroys all entities.
 */
World::~World() {
    m_ClientRegistry.clear();
    m_ServerRegistry.clear();
    spdlog::info("World: destroyed");
}

/**
 * @brief Updates all game logic for the current frame.
 * @param dt The time elapsed since the last frame in seconds.
 * @param net The network manager to handle sync.
 * @param renderer Pointer to the renderer instance.
 */
void World::Update(float dt, NetworkManager& net, Renderer* renderer) {
    SceneContext ctx{m_ServerRegistry, m_ClientRegistry, net, m_SceneManager, renderer, m_Physics, this};

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
     * @brief Handle per-frame logic updates.
     */
    m_SceneManager.LogicUpdate(ctx, dt);
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
    SceneContext ctx{m_ServerRegistry, m_ClientRegistry, net, m_SceneManager, renderer, m_Physics, this};
    m_SceneManager.Render(ctx, renderer);
}

/**
 * @brief Renders ImGui UI for the current world state.
 */
void World::OnImGuiRender(float dt, NetworkManager& net, Renderer* renderer) {
    SceneContext ctx{m_ServerRegistry, m_ClientRegistry, net, m_SceneManager, renderer, m_Physics, this};
    m_SceneManager.UIUpdate(ctx, dt);
}

/**
 * @brief Updates entity transforms from physics snapshots.
 * @param snapshots Vector of transform snapshots from the physics server.
 */
void World::ApplySnapshots(const std::vector<TransformSnapshot>& snapshots) {
    for (const auto& snapshot : snapshots) {
        // 1. Authoritative Server Update
        auto it = m_IDToEntity.find(snapshot.entityID);
        if (it != m_IDToEntity.end()) {
            auto sEntity = it->second;

            if (m_ServerRegistry.valid(sEntity) && m_ServerRegistry.any_of<PhysicsBodyComponent>(sEntity)) {
                glm::vec3 pos(snapshot.position.GetX(), snapshot.position.GetY(), snapshot.position.GetZ());
                JPH::Vec3 euler = snapshot.rotation.GetEulerAngles();
                glm::vec3 rot(glm::degrees(euler.GetX()), glm::degrees(euler.GetY()), glm::degrees(euler.GetZ()));

                if (auto* tf = m_ServerRegistry.try_get<TransformComponent>(sEntity)) {
                    tf->position = pos;
                    tf->rotation = rot;
                }

                // 2. Visual Client Update (for Host)
                // We MUST find the visual entity in the client registry that matches the server entity's netId.
                if (auto* net = m_ServerRegistry.try_get<NetworkedComponent>(sEntity)) {
                    uint32_t targetNetId = net->netId;
                    
                    auto clientView = m_ClientRegistry.view<NetworkedComponent, TransformComponent>();
                    for (auto cEntity : clientView) {
                        if (clientView.get<NetworkedComponent>(cEntity).netId == targetNetId) {
                            auto& cTf = clientView.get<TransformComponent>(cEntity);
                            cTf.position = pos;
                            cTf.rotation = rot;
                            break;
                        }
                    }
                }
            }
        }
    }
}

/**
 * @brief Registers an entity for physics snapshot synchronization.
 */
void World::RegisterPhysicsEntity(uint32_t id, entt::entity entity) {
    spdlog::debug("World: registering physics entity ID {} to entt entity {}", id, (uint32_t)entity);
    m_IDToEntity[id] = entity;
}

/**
 * @brief Unregisters an entity from physics synchronization.
 */
void World::UnregisterPhysicsEntity(uint32_t id) {
    spdlog::debug("World: unregistering physics ID {}", id);
    m_IDToEntity.erase(id);
}

void World::ClearPhysicsState() {
    spdlog::info("World: clearing all physics mappings and resetting ID counter");
    m_IDToEntity.clear();
    m_NextEntityID = 10000;
}

/**
 * @brief Performs a fixed-rate logic update.
 */
void World::FixedUpdate(float dt, NetworkManager& net, Renderer* renderer) {
    SceneContext ctx{m_ServerRegistry, m_ClientRegistry, net, m_SceneManager, renderer, m_Physics, this};
    m_SceneManager.FixedUpdate(ctx, dt);
}
