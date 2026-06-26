/**
 * @file ResourceSystem.h
 * @brief Assignment-driven worker state machine.
 *
 * Workers do NOT auto-find resources.  They wait Idle until the player
 * assigns them to a specific resource node (WORKER_ASSIGN packet → sets
 * CollectorComponent::assignedResourceNetId).  Unassigning (netId = 0) or
 * pressing S sends them back to the commander.
 *
 * State loop (server only, runs every fixed tick):
 *
 *   Idle → GoingToResource → Collecting → Returning → Depositing
 *       ↑_______________________________________↓   (if still assigned)
 *                                                  → ReturningToCommander → Idle
 *
 * Movement is issued via MovementOrderComponent; the existing
 * UpdateUnitMovement / A* pipeline handles pathfinding.
 */

#pragma once
#include <entt/entt.hpp>
#include "ResourceManager.h"

namespace ResourceSystem
{
    /**
     * @brief Runs one server tick of the worker AI.
     *
     * @param registry        Authoritative server registry.
     * @param resourceManager Used for Collect() calls.
     * @param dt              Fixed delta time in seconds.
     */
    void Update(entt::registry& registry, ResourceManager& resourceManager, float dt);

} // namespace ResourceSystem
