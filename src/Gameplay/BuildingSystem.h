/**
 * @file BuildingSystem.h
 * @brief System for handling building logic, such as upgrading and health.
 */

#pragma once
#include <entt/entt.hpp>
#include "Building_classes.h"

namespace BuildingSystem {
    /**
     * @brief Updates building states (upgrades, etc.)
     * @param registry The authoritative server registry.
     * @param dt Fixed delta time.
     */
    void Update(entt::registry& registry, float dt);

    /**
     * @brief Initiates an upgrade for a building if resources are available.
     * @param registry The registry containing the building and the player's base inventory.
     * @param buildingEntity The entity to upgrade.
     * @param baseEntity The base entity where resources are stored.
     * @return bool True if the upgrade was successfully started.
     */
    bool TryStartUpgrade(entt::registry& registry, entt::entity buildingEntity, entt::entity baseEntity);
}
