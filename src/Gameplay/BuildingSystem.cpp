/**
 * @file BuildingSystem.cpp
 * @brief Implementation of the building system logic.
 */

#include "BuildingSystem.h"
#include <spdlog/spdlog.h>

namespace BuildingSystem {

void Update(entt::registry& registry, float dt) {
    auto view = registry.view<BuildingComponent>();
    for (auto entity : view) {
        auto& building = view.get<BuildingComponent>(entity);

        if (building.isUpgrading) {
            building.upgradeTimer -= dt;
            if (building.upgradeTimer <= 0.0f) {
                building.isUpgrading = false;
                building.currentTier++;
                
                // Increase stats on upgrade
                building.maxHp += 50.0f;
                building.hp = building.maxHp;
                building.upgradedThisTick = true;
                
                spdlog::info("[BuildingSystem] Building upgraded to tier {}", building.currentTier);
            }
        }
    }
}

bool TryStartUpgrade(entt::registry& registry, entt::entity buildingEntity, entt::entity baseEntity) {
    auto* building = registry.try_get<BuildingComponent>(buildingEntity);
    auto* inventory = registry.try_get<ResourceInventory>(baseEntity);

    if (!building || !inventory) return false;
    if (building->isUpgrading || building->currentTier >= building->maxTier) return false;

    UpgradeRequirement req = BuildingComponent::GetNextTierCost(building->ownerClass, building->type, building->currentTier + 1);

    // Check resources
    if (inventory->pilze < req.cost.pilze ||
        inventory->beeren < req.cost.beeren ||
        inventory->nektar < req.cost.nektar ||
        inventory->samen < req.cost.samen ||
        inventory->insekten < req.cost.insekten ||
        inventory->fleisch < req.cost.fleisch) 
    {
        return false;
    }

    // Deduct resources
    inventory->pilze -= req.cost.pilze;
    inventory->beeren -= req.cost.beeren;
    inventory->nektar -= req.cost.nektar;
    inventory->samen -= req.cost.samen;
    inventory->insekten -= req.cost.insekten;
    inventory->fleisch -= req.cost.fleisch;

    // Start upgrade
    building->isUpgrading = true;
    building->upgradeTimer = req.buildTime;
    building->currentUpgradeTime = req.buildTime;

    spdlog::info("[BuildingSystem] Started upgrade for building. Time: {}s", req.buildTime);

    return true;
}

} // namespace BuildingSystem
