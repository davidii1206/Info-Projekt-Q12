/**
 * @file ResourceManager.h
 * @brief Server-side manager for resource node spawning and lifecycle.
 *
 * ResourceManager owns the list of static spawn-points for permanent resources
 * and handles:
 *  - Initial spawn on map start
 *  - Respawn timer countdown for depleted permanent nodes
 *  - Collection logic called by the CollectorSystem
 *
 * Usage (inside GameScene::OnEnter, server only):
 * @code
 *   m_ResourceManager.Init(ctx.serverRegistry);
 *   m_ResourceManager.SpawnPermanentResources(ctx.serverRegistry);
 * @endcode
 *
 * Then call every fixed tick:
 * @code
 *   m_ResourceManager.Update(ctx.serverRegistry, dt);
 * @endcode
 */

#pragma once
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <vector>
#include "ResourceTypes.h"
#include "../Core/WorldManager.h"

/**
 * @struct ResourceSpawnPoint
 * @brief Defines a fixed world position where a permanent resource node spawns.
 */
struct ResourceSpawnPoint {
    glm::vec3    position;     ///< World-space position.
    ResourceType type;         ///< Which resource type spawns here.
    int          amount;       ///< How many units are available per cycle.
    float        respawnTime;  ///< Seconds until the node refills.
};

/**
 * @class ResourceManager
 * @brief Manages creation, depletion, and respawn of resource nodes.
 */
class ResourceManager {
public:
    ResourceManager() = default;

    /**
     * @brief Registers the fixed spawn-point list for this map.
     *
     * Call once from GameScene::OnEnter() before SpawnPermanentResources().
     * The default list contains representative spawn points; replace or
     * extend this list once real map coordinates are known.
     */
    void Init();

    /**
     * @brief Procedurally generates spawn points with biome-aware placement.
     *
     * Each faction's signature resource is concentrated near its territory's
     * spawn point.  Holz (universal) is spread randomly across the whole map.
     * Non-signature types get a sparse global scatter so every map has all
     * resource types reachable.
     *
     * @param world   Live WorldManager — used to read territory faction data.
     * @param seed    RNG seed for deterministic node placement.
     */
    void GenerateForWorld(const WorldManager& world, uint32_t seed = 0xC0FFEE);

    /**
     * @brief Spawns all permanent resource nodes at their defined positions.
     *
     * Creates EnTT entities with TransformComponent + ResourceComponent.
     * Must be called on the server registry only.
     *
     * @param registry The authoritative server registry.
     */
    void SpawnPermanentResources(entt::registry& registry);

    /**
     * @brief Ticks respawn timers for depleted permanent nodes.
     *
     * Call every server fixed-update tick.
     *
     * @param registry The authoritative server registry.
     * @param dt       Delta time in seconds.
     */
    void Update(entt::registry& registry, float dt);

    /**
     * @brief Tries to collect from a resource node.
     *
     * Reduces the node's amount by 1 (or marks it depleted if empty).
     * Returns the resource type and amount actually collected (0 if depleted).
     *
     * @param registry  The authoritative server registry.
     * @param node      The resource entity to collect from.
     * @param outType   Set to the collected resource type.
     * @return          Amount collected (0 if node was already depleted).
     */
    int Collect(entt::registry& registry, entt::entity node, ResourceType& outType);

    /**
     * @brief Spawns a non-permanent Fleisch drop at the given world position.
     *
     * Call this when an enemy is killed.  The entity has `permanent = false`
     * and will be destroyed automatically by Update() once `dropLifetime`
     * seconds have elapsed.
     *
     * @param registry     The authoritative server registry.
     * @param position     World-space position of the drop.
     * @param amount       Number of Fleisch units in this drop (default 1).
     * @param dropLifetime Seconds before the drop disappears (default 15 s).
     */
    void SpawnMeatDrop(entt::registry& registry,
                       glm::vec3       position,
                       int             amount      = 1,
                       float           dropLifetime = 15.f);

    /**
     * @brief Returns a read-only view of all defined spawn points.
     */
    const std::vector<ResourceSpawnPoint>& GetSpawnPoints() const { return m_SpawnPoints; }

private:
    /// Fixed spawn point definitions for the current map.
    std::vector<ResourceSpawnPoint> m_SpawnPoints;

    /// Rebuilds a depleted node back to full amount.
    void RespawnNode(entt::registry& registry, entt::entity entity);
};
