/**
 * @file BuildingSystem.cpp
 * @brief Implementation of the building system logic.
 */

#include "BuildingSystem.h"
#include "Components.h"
#include "ResourceTypes.h"
#include "Bug_classes.h"
#include "UpgradeSystem.h"
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

                // Increase stats on upgrade – scale with tribe bonuses
                float hpMul = 1.0f;
                auto data = GetTribeBuildingData(building.ownerClass);
                if (building.type == BuildingType::Defense)
                    hpMul = data.defenseHpMul;

                building.maxHp += 50.0f * hpMul;
                building.hp = building.maxHp;
                building.upgradedThisTick = true;

                spdlog::info("[BuildingSystem] Building upgraded to tier {}", building.currentTier);
            }
        }
    }

    // Process conversions
    UpdateConversions(registry, dt);

    // Process barracks spawn queues
    UpdateBarracks(registry, dt);
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
        inventory->fleisch < req.cost.fleisch ||
        inventory->holz < req.cost.holz)
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
    inventory->holz -= req.cost.holz;

    // Start upgrade
    building->isUpgrading = true;
    building->upgradeTimer = req.buildTime;
    building->currentUpgradeTime = req.buildTime;

    spdlog::info("[BuildingSystem] Started upgrade for building. Time: {}s", req.buildTime);

    return true;
}

void UpdateConversions(entt::registry& registry, float dt) {
    // Find all conversion buildings with ConversionComponent
    auto view = registry.view<BuildingComponent, ConversionComponent>();
    for (auto entity : view) {
        auto& building = registry.get<BuildingComponent>(entity);
        auto& conv = registry.get<ConversionComponent>(entity);

        if (building.destroyed || building.isUpgrading) {
            conv.active = false;
            continue;
        }

        // Find the team's base inventory
        entt::entity baseInv = entt::null;
        auto invView = registry.view<ResourceInventory, BuildingComponent>();
        for (auto be : invView) {
            auto& bc = invView.get<BuildingComponent>(be);
            if (bc.teamId == building.teamId && !bc.destroyed &&
                (bc.type == BuildingType::Main || bc.type == BuildingType::Storage)) {
                baseInv = be;
                break;
            }
        }

        if (baseInv == entt::null) {
            conv.active = false;
            continue;
        }

        auto& inv = registry.get<ResourceInventory>(baseInv);

        // Check if we have enough input resources
        if (inv.Get(conv.inputType) >= conv.inputAmt) {
            conv.active = true;
            float rateMul = UpgradeSystem::GetConversionRateMul(building.teamId);
            conv.timer -= dt * rateMul;

            if (conv.timer <= 0.f) {
                // Consume input
                inv.Add(conv.inputType, -conv.inputAmt);
                // Produce output
                inv.Add(conv.outputType, conv.outputAmt);
                conv.timer = conv.cycleTime;

                spdlog::debug("[BuildingSystem] Conversion: {} x {} -> {} x {}",
                              conv.inputAmt, ResourceTypeName(conv.inputType),
                              conv.outputAmt, ResourceTypeName(conv.outputType));
            }
        } else {
            conv.active = false;
            conv.timer = conv.cycleTime; // reset timer when disabled
        }
    }
}

void UpdateBarracks(entt::registry& registry, float dt) {
    auto view = registry.view<BuildingComponent, BarracksComponent>();
    for (auto entity : view) {
        auto& building = registry.get<BuildingComponent>(entity);
        auto& barracks = registry.get<BarracksComponent>(entity);

        if (building.destroyed || building.isUpgrading) continue;
        if (barracks.queue.empty()) continue;

        // Process the first job in the queue
        auto& job = barracks.queue.front();
        float speedMul = 1.f;
        auto data = GetTribeBuildingData(building.ownerClass);
        speedMul = data.unitProductionSpeed;
        float upgradeMul = UpgradeSystem::GetSpawnRateMul(building.teamId);

        job.timer -= dt * speedMul * upgradeMul;
        if (job.timer <= 0.f) {
            // Mark the job as completed so the GameScene tick can pick it up
            // and actually create + broadcast the unit entity (BuildingSystem
            // doesn't know about networking).
            spdlog::debug("[BuildingSystem] Barracks completed spawn job (tier {}, role {})",
                          job.tier, static_cast<int>(job.role));
            UnitRole completedRole = job.role;
            barracks.queue.erase(barracks.queue.begin());
            if (completedRole == UnitRole::Worker)
                ++barracks.completedWorkerSpawns;
            else
                ++barracks.completedSpawns;
        }
    }
}

int GetTeamStorageBonus(entt::registry& registry, uint32_t teamId) {
    int total = 0;
    auto view = registry.view<BuildingComponent, StorageComponent>();
    for (auto entity : view) {
        auto& bc = view.get<BuildingComponent>(entity);
        auto& sc = view.get<StorageComponent>(entity);
        if (bc.teamId == teamId && !bc.destroyed && sc.isActive) {
            // Scale bonus by tier
            float tierMul = 1.f + (bc.currentTier - 1) * 0.5f;
            auto data = GetTribeBuildingData(bc.ownerClass);
            total += static_cast<int>(sc.capacityBonus * tierMul * data.storageCapacityMul);
        }
    }
    float upgradeMul = UpgradeSystem::GetStorageMul(teamId);
    return static_cast<int>(total * upgradeMul);
}

} // namespace BuildingSystem
