/**
 * @file ResourceSystem.h
 * @brief ECS system: collectors auto-gather nearby resources and return to base.
 *
 * Call ResourceSystem::Update() every server fixed-tick.
 *
 * Collector behaviour (state machine):
 *  1. SEEKING  – walks toward nearest non-depleted resource within detectRadius
 *  2. COLLECTING – within collectRadius: calls ResourceManager::Collect()
 *  3. RETURNING – walks back to the nearest base entity
 *  4. DEPOSITING – within depositRadius of base: transfers ResourceInventory to base
 *
 * The system operates purely on the authoritative server registry and is
 * intentionally decoupled from networking – the position updates broadcast
 * through the normal EntitySnapshot path.
 */

#pragma once
#include <entt/entt.hpp>
#include "ResourceManager.h"

namespace ResourceSystem
{
    /**
     * @brief Runs one tick of the resource collection AI.
     *
     * @param registry        Authoritative server registry.
     * @param resourceManager The ResourceManager for Collect() calls.
     * @param dt              Fixed delta time in seconds.
     */
    void Update(entt::registry& registry, ResourceManager& resourceManager, float dt);

} // namespace ResourceSystem
