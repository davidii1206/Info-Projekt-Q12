/**
 * @file Building_classes.h
 * @brief Components and enums for the building system.
 */

#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "ResourceTypes.h"
#include "Bug_classes.h"

/**
 * @enum BuildingType
 * @brief Categorizes buildings by their primary function.
 */
enum class BuildingType : uint8_t {
    Attack,
    Defense,
    Resource,
    Outpost,
    Main,
    Offense,
    Upgrade,
    Infrastructure,
};

/**
 * @struct UpgradeRequirement
 * @brief Defines the cost to upgrade a building to the next tier.
 */
struct UpgradeRequirement {
    ResourceInventory cost;
    float buildTime = 5.0f; // Seconds to complete upgrade
};

/**
 * @struct BuildingComponent
 * @brief Primary component for building logic, HP, and tiers.
 */
struct BuildingComponent {
    BuildingType type;
    BugClass ownerClass;
    uint32_t teamId = 0;
    uint32_t tier  = 1;

    float hp    = 100.0f;
    float maxHp = 100.0f;

    bool destroyed = false;

    uint32_t currentTier = 1;
    uint32_t maxTier = 3;

    bool isUpgrading      = false;
    float upgradeTimer    = 0.0f;
    float currentUpgradeTime = 0.0f;
    bool upgradedThisTick = false;

    BuildingComponent() = default;
    BuildingComponent(BuildingType t, uint32_t tid, uint32_t tier_, float initialHp, float maxInitialHp, bool destroyed_)
        : type(t), ownerClass(BugClass::Termites), teamId(tid), tier(tier_),
          hp(initialHp), maxHp(maxInitialHp), destroyed(destroyed_) {}

    /**
     * @brief Gets the upgrade requirements for the next tier.
     * 
     * @return UpgradeRequirement for next tier.
     */
    static UpgradeRequirement GetNextTierCost(BugClass bc, BuildingType type, uint32_t nextTier) {
        UpgradeRequirement req;
        if (nextTier > 3) return req;

        // Simplified logic: Base cost scaled by tier and slightly varied by bug class
        int baseCost = static_cast<int>(nextTier) * 10;
        
        // Termites (Builders) get a discount
        if (bc == BugClass::Termites) {
            baseCost = static_cast<int>(baseCost * 0.8f);
        }

        switch (type) {
            case BuildingType::Main:
                req.cost.pilze = baseCost * 3;
                req.cost.beeren = baseCost * 2;
                req.buildTime = 15.0f * nextTier;
                break;
            case BuildingType::Attack:
                req.cost.fleisch = baseCost * 2;
                req.cost.samen = baseCost;
                req.buildTime = 8.0f * nextTier;
                break;
            case BuildingType::Defense:
                req.cost.pilze = baseCost * 2;
                req.cost.samen = baseCost;
                req.buildTime = 10.0f * nextTier;
                break;
            case BuildingType::Resource:
                req.cost.nektar = baseCost * 2;
                req.cost.beeren = baseCost;
                req.buildTime = 5.0f * nextTier;
                break;
            case BuildingType::Outpost:
                req.cost.pilze = baseCost;
                req.cost.nektar = baseCost;
                req.buildTime = 12.0f * nextTier;
                break;
        }
        return req;
    }
};
