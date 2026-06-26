/**
 * @file ResourceSystem.cpp
 * @brief Assignment-driven worker state machine.
 *
 * Unassigned workers (Idle / ReturningToCommander) follow the commander
 * like a loose entourage — they close in when the commander moves away and
 * stop when within FOLLOW_RADIUS.  Assigning a worker to a resource node
 * sends it through GoingToResource → Collecting → Returning → Depositing,
 * then back to following the commander.
 *
 *   Idle ←──────────────────────────── ReturningToCommander
 *    │  (both states follow the commander)
 *    └──[assigned]──► GoingToResource → Collecting → Returning → Depositing
 *                         ↑_______________________↓   (if still assigned)
 *                                                    → ReturningToCommander
 *
 * Movement is issued via MovementOrderComponent; the existing
 * UpdateUnitMovement / A* pipeline handles the actual pathfinding.
 */

#include "ResourceSystem.h"
#include "Components.h"
#include "ResourceTypes.h"
#include "Bug_classes.h"
#include "UpgradeSystem.h"
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>
#include <unordered_map>

namespace ResourceSystem
{

namespace
{
    constexpr float DEPOSIT_RADIUS  = 3.f;  ///< Distance to base that triggers deposit.
    constexpr float FOLLOW_RADIUS   = 5.f;  ///< Workers stop following when this close to commander.
    constexpr float FOLLOW_DEADZONE = 1.5f; ///< Don't re-issue a move order until drift exceeds this.

    /// Finds the player entity (commander) for the given team.
    entt::entity FindCommander(entt::registry& registry, uint32_t teamId)
    {
        auto view = registry.view<TransformComponent, PlayerComponent>();
        for (auto e : view) {
            if (view.get<PlayerComponent>(e).playerId == teamId)
                return e;
        }
        return entt::null;
    }

    /// Moves `mo` toward commander if the worker has drifted outside FOLLOW_RADIUS,
    /// stops if already close.  Returns true if the commander was found.
    bool FollowCommander(entt::registry& registry, uint32_t teamId,
                         const glm::vec3& pos, MovementOrderComponent& mo)
    {
        entt::entity cmd = FindCommander(registry, teamId);
        if (cmd == entt::null) { mo.active = false; return false; }
        const auto& cmdPos = registry.get<TransformComponent>(cmd).position;
        float dist = std::sqrt((cmdPos.x-pos.x)*(cmdPos.x-pos.x) +
                               (cmdPos.z-pos.z)*(cmdPos.z-pos.z));
        if (dist > FOLLOW_RADIUS) {
            mo.destination = cmdPos;
            mo.active = true;
        } else if (dist < FOLLOW_RADIUS - FOLLOW_DEADZONE) {
            mo.active = false;
        }
        return true;
    }

    /// Finds the first base entity owned by teamId (prefers BaseComponent, falls
    /// back to a non-destroyed MainBase BuildingComponent).
    entt::entity FindBase(entt::registry& registry, uint32_t teamId)
    {
        auto bView = registry.view<TransformComponent, BaseComponent>();
        for (auto e : bView) {
            if (bView.get<BaseComponent>(e).teamId == teamId)
                return e;
        }
        auto bldView = registry.view<TransformComponent, BuildingComponent>();
        for (auto e : bldView) {
            auto& bc = bldView.get<BuildingComponent>(e);
            if (bc.teamId == teamId && !bc.destroyed)
                return e;
        }
        return entt::null;
    }

    /// XZ distance between two world positions.
    float DistXZ(const glm::vec3& a, const glm::vec3& b)
    {
        float dx = b.x - a.x, dz = b.z - a.z;
        return std::sqrt(dx * dx + dz * dz);
    }

} // namespace

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void Update(entt::registry& registry, ResourceManager& resourceManager, float dt)
{
    // Build a netId → resource entity lookup once per tick.
    std::unordered_map<uint32_t, entt::entity> resNetMap;
    {
        auto resView = registry.view<NetworkedComponent, ResourceComponent>();
        for (auto e : resView)
            resNetMap[resView.get<NetworkedComponent>(e).netId] = e;
    }

    auto view = registry.view<TransformComponent, CollectorComponent,
                               ResourceInventory, MovementOrderComponent, UnitComponent>();

    for (auto collectorEntity : view)
    {
        auto& tf        = view.get<TransformComponent>(collectorEntity);
        auto& collector = view.get<CollectorComponent>(collectorEntity);
        auto& inventory = view.get<ResourceInventory>(collectorEntity);
        auto& mo        = view.get<MovementOrderComponent>(collectorEntity);
        const uint32_t teamId = view.get<UnitComponent>(collectorEntity).teamId;

        // Tick harvest cooldown, boosted by any gather-rate upgrade.
        if (collector.collectCooldown > 0.f)
            collector.collectCooldown -= dt * UpgradeSystem::GetGatherRateMul(teamId);

        switch (collector.state)
        {
        // -------------------------------------------------------------------
        case WorkerState::Idle:
        {
            if (collector.assignedResourceNetId != 0) {
                mo.active = false;
                collector.state = WorkerState::GoingToResource;
                break;
            }
            // No assignment — loosely follow the commander.
            FollowCommander(registry, teamId, tf.position, mo);
            break;
        }

        // -------------------------------------------------------------------
        case WorkerState::GoingToResource:
        {
            if (collector.assignedResourceNetId == 0) {
                mo.active = false;
                collector.state = WorkerState::ReturningToCommander;
                break;
            }
            auto rit = resNetMap.find(collector.assignedResourceNetId);
            if (rit == resNetMap.end() || !registry.valid(rit->second)) {
                collector.assignedResourceNetId = 0;
                mo.active = false;
                collector.state = WorkerState::ReturningToCommander;
                break;
            }
            entt::entity resEnt = rit->second;
            auto& res   = registry.get<ResourceComponent>(resEnt);
            auto& resTf = registry.get<TransformComponent>(resEnt);

            if (res.depleted) {
                // Node is respawning — wait in place.
                mo.active = false;
                break;
            }

            float dist = DistXZ(tf.position, resTf.position);
            if (dist <= collector.collectRadius) {
                mo.active = false;
                collector.state = WorkerState::Collecting;
            } else {
                mo.destination = resTf.position;
                mo.active = true;
            }
            break;
        }

        // -------------------------------------------------------------------
        case WorkerState::Collecting:
        {
            if (collector.assignedResourceNetId == 0) {
                mo.active = false;
                collector.state = collector.carryingLoad
                    ? WorkerState::Returning : WorkerState::ReturningToCommander;
                break;
            }
            auto rit = resNetMap.find(collector.assignedResourceNetId);
            if (rit == resNetMap.end() || !registry.valid(rit->second)) {
                collector.assignedResourceNetId = 0;
                mo.active = false;
                collector.state = collector.carryingLoad
                    ? WorkerState::Returning : WorkerState::ReturningToCommander;
                break;
            }
            entt::entity resEnt = rit->second;
            auto& res   = registry.get<ResourceComponent>(resEnt);

            if (res.depleted) {
                // Wait for respawn — reuse collectCooldown as a poll timer.
                if (collector.collectCooldown <= 0.f)
                    collector.collectCooldown = 1.f;
                break;
            }

            if (collector.collectCooldown <= 0.f) {
                ResourceType collectedType;
                int amount = resourceManager.Collect(registry, resEnt, collectedType);
                if (amount > 0) {
                    inventory.Add(collectedType, amount);
                    collector.collectCooldown = collector.collectRate;
                    collector.carryingLoad    = true;
                    collector.state           = WorkerState::Returning;
                }
            }
            break;
        }

        // -------------------------------------------------------------------
        case WorkerState::Returning:
        {
            entt::entity base = FindBase(registry, teamId);
            if (base == entt::null) {
                inventory           = ResourceInventory{};
                collector.carryingLoad = false;
                collector.state     = WorkerState::ReturningToCommander;
                break;
            }
            const auto& baseTf = registry.get<TransformComponent>(base);
            float dist = DistXZ(tf.position, baseTf.position);
            if (dist <= DEPOSIT_RADIUS) {
                mo.active = false;
                collector.state = WorkerState::Depositing;
            } else {
                mo.destination = baseTf.position;
                mo.active = true;
            }
            break;
        }

        // -------------------------------------------------------------------
        case WorkerState::Depositing:
        {
            entt::entity base = FindBase(registry, teamId);
            if (base != entt::null) {
                if (auto* baseInv = registry.try_get<ResourceInventory>(base)) {
                    baseInv->Add(ResourceType::Pilze,    inventory.pilze);
                    baseInv->Add(ResourceType::Beeren,   inventory.beeren);
                    baseInv->Add(ResourceType::Nektar,   inventory.nektar);
                    baseInv->Add(ResourceType::Samen,    inventory.samen);
                    baseInv->Add(ResourceType::Insekten, inventory.insekten);
                    baseInv->Add(ResourceType::Fleisch,  inventory.fleisch);
                    baseInv->Add(ResourceType::Holz,     inventory.holz);
                }
            }
            inventory              = ResourceInventory{};
            collector.carryingLoad = false;

            // If the player's assignment is still valid, loop back for more.
            if (collector.assignedResourceNetId != 0) {
                auto rit = resNetMap.find(collector.assignedResourceNetId);
                if (rit != resNetMap.end() && registry.valid(rit->second)) {
                    collector.state = WorkerState::GoingToResource;
                    break;
                }
                collector.assignedResourceNetId = 0;
            }
            collector.state = WorkerState::ReturningToCommander;
            break;
        }

        // -------------------------------------------------------------------
        case WorkerState::ReturningToCommander:
        {
            if (collector.assignedResourceNetId != 0) {
                // Re-assigned while walking back — go immediately.
                collector.state = WorkerState::GoingToResource;
                break;
            }
            // Walk back; switch to Idle (which also follows) once close enough.
            entt::entity cmd = FindCommander(registry, teamId);
            if (cmd == entt::null) {
                mo.active = false;
                collector.state = WorkerState::Idle;
                break;
            }
            const auto& cmdPos = registry.get<TransformComponent>(cmd).position;
            float dist = DistXZ(tf.position, cmdPos);
            if (dist <= FOLLOW_RADIUS) {
                mo.active = false;
                collector.state = WorkerState::Idle;
            } else {
                mo.destination = cmdPos;
                mo.active = true;
            }
            break;
        }

        } // switch
    }
}

} // namespace ResourceSystem
