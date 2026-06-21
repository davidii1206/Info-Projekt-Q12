/**
 * @file ResourceSystem.cpp
 * @brief Implementation of the ResourceSystem ECS tick.
 */

#include "ResourceSystem.h"
#include "Components.h"
#include "ResourceTypes.h"
#include "Bug_classes.h"
#include "UpgradeSystem.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>
#include <limits>
#include <unordered_set>

namespace ResourceSystem
{

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{
    constexpr float DEPOSIT_RADIUS  = 2.f;  ///< Distance to base at which deposit triggers.
    constexpr float DETECT_RADIUS   = 15.f; ///< Distance within which collectors notice resources.
    constexpr float COLLECTOR_SPEED = 4.f;  ///< Units per second.

    /**
     * @brief Moves `pos` toward `target` by up to `speed * dt` units.
     * @return true if the target was reached.
     */
    bool MoveToward(glm::vec3& pos, const glm::vec3& target, float speed, float dt)
    {
        glm::vec3 diff = target - pos;
        float dist = glm::length(diff);
        if (dist < 0.05f) return true;

        glm::vec3 dir = diff / dist;
        float step = std::min(speed * dt, dist);
        pos += dir * step;
        return step >= dist;
    }

    /**
     * @brief Determines the BugClass for a collector entity.
     *
     * Checks UnitComponent and PlayerComponent for bug class info.
     * Falls back to a heuristic based on teamId (team 0 → Termites, team 1 → Ants).
     */
    BugClass GetCollectorBugClass(entt::registry& registry, entt::entity e) {
        if (auto* uc = registry.try_get<UnitComponent>(e))
            return uc->bugClass;
        if (auto* pc = registry.try_get<PlayerComponent>(e))
            return pc->bugClass;
        return BugClass::Ants;
    }

    /**
     * @brief Finds the nearest non-depleted resource entity within detectRadius
     *        that matches one of the given acceptable resource types.
     * @return entt::null if none found.
     */
    entt::entity FindNearestResource(entt::registry& registry,
                                     const glm::vec3& from,
                                     float            maxDist,
                                     const std::unordered_set<ResourceType>& acceptable)
    {
        entt::entity best = entt::null;
        float bestDist = maxDist * maxDist;

        auto view = registry.view<TransformComponent, ResourceComponent>();
        for (auto e : view) {
            const auto& res = view.get<ResourceComponent>(e);
            if (res.depleted) continue;
            // Skip resources this bug class cannot eat
            if (!acceptable.empty() && acceptable.find(res.type) == acceptable.end())
                continue;

            const auto& tf = view.get<TransformComponent>(e);
            glm::vec3 diff3 = tf.position - from;
            float d2 = glm::dot(diff3, diff3);
            if (d2 < bestDist) {
                bestDist = d2;
                best = e;
            }
        }
        return best;
    }

    /**
     * @brief Finds the nearest base entity for the given teamId.
     */
    entt::entity FindBase(entt::registry& registry, uint32_t teamId)
    {
        auto view = registry.view<TransformComponent, BaseComponent>();
        for (auto e : view) {
            if (view.get<BaseComponent>(e).teamId == teamId)
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
} // namespace

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void Update(entt::registry& registry, ResourceManager& resourceManager, float dt)
{
    auto view = registry.view<TransformComponent, CollectorComponent, ResourceInventory>();

    for (auto collectorEntity : view)
    {
        auto& tf        = view.get<TransformComponent>(collectorEntity);
        auto& collector = view.get<CollectorComponent>(collectorEntity);
        auto& inventory = view.get<ResourceInventory>(collectorEntity);

        // Determine team and bug class
        BugClass bc = GetCollectorBugClass(registry, collectorEntity);
        uint32_t teamId = 0;
        if (auto* uc = registry.try_get<UnitComponent>(collectorEntity))
            teamId = uc->teamId;
        else if (auto* pc = registry.try_get<PlayerComponent>(collectorEntity))
            teamId = pc->playerId;

        auto edibleVec = GetEdibleResources(bc);
        std::unordered_set<ResourceType> acceptable(edibleVec.begin(), edibleVec.end());

        // Tick cooldown (scaled by upgrade gather-rate bonus)
        if (collector.collectCooldown > 0.f) {
            collector.collectCooldown -= dt * UpgradeSystem::GetGatherRateMul(teamId);
        }

        if (!collector.carryingLoad)
        {
            // ----------------------------------------------------------------
            // Phase 1: SEEKING / COLLECTING
            // ----------------------------------------------------------------
            entt::entity target = FindNearestResource(registry, tf.position, DETECT_RADIUS, acceptable);

            if (target == entt::null) continue; // nothing suitable nearby

            const auto& resTf = registry.get<TransformComponent>(target);
            float distToResource = glm::distance(tf.position, resTf.position);

            if (distToResource > collector.collectRadius)
            {
                MoveToward(tf.position, resTf.position, COLLECTOR_SPEED, dt);
            }
            else if (collector.collectCooldown <= 0.f)
            {
                ResourceType collectedType;
                int amount = resourceManager.Collect(registry, target, collectedType);
                if (amount > 0)
                {
                    inventory.Add(collectedType, amount);
                    collector.collectCooldown = collector.collectRate;
                    collector.carryingLoad    = true;

                    spdlog::debug("[ResourceSystem] {} collector picked up {} x {}",
                                  BugClassName(bc), amount, ResourceTypeName(collectedType));
                }
            }
        }
        else
        {
            // ----------------------------------------------------------------
            // Phase 2: RETURNING to base
            // ----------------------------------------------------------------
            entt::entity base = FindBase(registry, teamId);
            if (base == entt::null) {
                collector.carryingLoad = false;
                continue;
            }

            const auto& baseTf = registry.get<TransformComponent>(base);
            float distToBase   = glm::distance(tf.position, baseTf.position);

            if (distToBase > DEPOSIT_RADIUS)
            {
                MoveToward(tf.position, baseTf.position, COLLECTOR_SPEED, dt);
            }
            else
            {
                // ----------------------------------------------------------------
                // Phase 3: DEPOSITING
                // ----------------------------------------------------------------
                auto* baseInv = registry.try_get<ResourceInventory>(base);
                if (baseInv)
                {
                    baseInv->Add(ResourceType::Pilze,    inventory.pilze);
                    baseInv->Add(ResourceType::Beeren,   inventory.beeren);
                    baseInv->Add(ResourceType::Nektar,   inventory.nektar);
                    baseInv->Add(ResourceType::Samen,    inventory.samen);
                    baseInv->Add(ResourceType::Insekten, inventory.insekten);
                    baseInv->Add(ResourceType::Fleisch,  inventory.fleisch);

                    spdlog::debug("[ResourceSystem] {} collector deposited load at base (team {})",
                                  BugClassName(bc), teamId);
                }

                inventory = ResourceInventory{};
                collector.carryingLoad = false;
            }
        }
    }
}

} // namespace ResourceSystem
