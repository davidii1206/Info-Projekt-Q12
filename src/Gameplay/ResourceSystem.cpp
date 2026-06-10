/**
 * @file ResourceSystem.cpp
 * @brief Implementation of the ResourceSystem ECS tick.
 */

#include "ResourceSystem.h"
#include "Components.h"
#include "ResourceTypes.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>
#include <limits>

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
     * @brief Finds the nearest non-depleted resource entity within detectRadius.
     * @return entt::null if none found.
     */
    entt::entity FindNearestResource(entt::registry& registry,
                                     const glm::vec3& from,
                                     float            maxDist)
    {
        entt::entity best = entt::null;
        float bestDist = maxDist * maxDist; // compare squared distances

        auto view = registry.view<TransformComponent, ResourceComponent>();
        for (auto e : view) {
            const auto& res = view.get<ResourceComponent>(e);
            if (res.depleted) continue;

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
     * @return entt::null if none found.
     */
    entt::entity FindBase(entt::registry& registry, uint32_t teamId)
    {
        auto view = registry.view<TransformComponent, BaseComponent>();
        for (auto e : view) {
            if (view.get<BaseComponent>(e).teamId == teamId)
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
    // Collectors need: TransformComponent, CollectorComponent, ResourceInventory, PlayerComponent(teamId via playerId)
    auto view = registry.view<TransformComponent, CollectorComponent, ResourceInventory>();

    for (auto collectorEntity : view)
    {
        auto& tf        = view.get<TransformComponent>(collectorEntity);
        auto& collector = view.get<CollectorComponent>(collectorEntity);
        auto& inventory = view.get<ResourceInventory>(collectorEntity);

        // Tick cooldown
        if (collector.collectCooldown > 0.f) {
            collector.collectCooldown -= dt;
        }

        if (!collector.carryingLoad)
        {
            // ----------------------------------------------------------------
            // Phase 1: SEEKING / COLLECTING
            // ----------------------------------------------------------------
            entt::entity target = FindNearestResource(registry, tf.position, DETECT_RADIUS);

            if (target == entt::null) continue; // nothing nearby, wait

            const auto& resTf = registry.get<TransformComponent>(target);
            float distToResource = glm::distance(tf.position, resTf.position);

            if (distToResource > collector.collectRadius)
            {
                // Move toward resource
                MoveToward(tf.position, resTf.position, COLLECTOR_SPEED, dt);
            }
            else if (collector.collectCooldown <= 0.f)
            {
                // Collect
                ResourceType collectedType;
                int amount = resourceManager.Collect(registry, target, collectedType);
                if (amount > 0)
                {
                    inventory.Add(collectedType, amount);
                    collector.collectCooldown = collector.collectRate;
                    collector.carryingLoad    = true;

                    spdlog::debug("[ResourceSystem] Collector picked up {} x {}",
                                  amount, ResourceTypeName(collectedType));
                }
            }
        }
        else
        {
            // ----------------------------------------------------------------
            // Phase 2: RETURNING to base
            // ----------------------------------------------------------------

            // Determine team – use PlayerComponent if present, else team 0
            uint32_t teamId = 0;
            if (auto* pc = registry.try_get<PlayerComponent>(collectorEntity))
                teamId = pc->playerId; // approximation; replace with TeamComponent later

            entt::entity base = FindBase(registry, teamId);
            if (base == entt::null) {
                // No base found – drop load and go back to seeking
                collector.carryingLoad = false;
                continue;
            }

            const auto& baseTf = registry.get<TransformComponent>(base);
            float distToBase   = glm::distance(tf.position, baseTf.position);

            if (distToBase > DEPOSIT_RADIUS)
            {
                // Move toward base
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

                    spdlog::debug("[ResourceSystem] Collector deposited load at base (team {})", teamId);
                }

                // Reset inventory
                inventory = ResourceInventory{};
                collector.carryingLoad = false;
            }
        }
    }
}

} // namespace ResourceSystem
