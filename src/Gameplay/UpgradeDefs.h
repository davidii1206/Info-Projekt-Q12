#pragma once
#include <cstdint>
#include <vector>
#include "Building_classes.h"
#include "ResourceTypes.h"

/// Unique identifier for an upgrade path.
using UpgradePathID = uint32_t;

/// Types of conditions an upgrade can require.
enum class UpgradeReqType : uint8_t {
    Resources,       ///< Must have enough resources in the team stockpile.
    KillCount,       ///< Team must have killed at least N enemies.
    BuildingCount,   ///< Team must have built N buildings of a given type.
    HasUpgrade,      ///< Another upgrade path must be completed first.
    TimeElapsed,     ///< Game time must have advanced at least N seconds.
};

/// One requirement that must be met before an upgrade can be taken.
struct UpgradeRequirementDef {
    UpgradeReqType type = UpgradeReqType::Resources;

    /// For Resources: the minimum amounts needed.
    ResourceInventory cost;

    /// For KillCount, BuildingCount, TimeElapsed: the threshold.
    int value = 0;

    /// For BuildingCount: the building type to count.
    BuildingType targetBuilding = BuildingType::Main;

    /// For HasUpgrade: the prerequisite upgrade path ID.
    UpgradePathID prerequisite = 0;
};

/// Passive bonuses granted by completing an upgrade path.
struct UpgradeEffectDef {
    // Unit stat modifiers (multiplied into base values)
    float unitDamageMul     = 1.0f;
    float unitDefenseMul    = 1.0f;
    float unitSpeedMul      = 1.0f;

    // Building modifiers
    float buildingHpMul     = 1.0f;
    float buildingCostMul   = 1.0f;

    // Economy modifiers
    float resourceGatherRateMul = 1.0f;
    float resourceStorageMul    = 1.0f;

    // Production modifiers
    float unitSpawnRateMul  = 1.0f;
    float conversionRateMul = 1.0f;

    // Unlocks (Count = nothing unlocked)
    BuildingType unlockBuilding       = BuildingType::Count;
    SpecialBuildingType unlockSpecial = SpecialBuildingType::Count;
};

/// An upgrade path: one purchaseable upgrade with requirements and effects.
struct UpgradePathDef {
    UpgradePathID id;
    const char*   name        = "Unnamed Upgrade";
    const char*   description = "";

    /// Building type this upgrade applies to (Main → base upgrade, Count → base upgrade)
    BuildingType appliesTo    = BuildingType::Count;

    std::vector<UpgradeRequirementDef> requirements;
    UpgradeEffectDef effects;
};

// ---------------------------------------------------------------------------
// Global registry access
// ---------------------------------------------------------------------------

/// Returns all upgrade path definitions.
const std::vector<UpgradePathDef>& GetAllUpgradeDefs();

/// Finds a specific upgrade path definition by ID, or nullptr if not found.
const UpgradePathDef* FindUpgradeDef(UpgradePathID id);

/// Returns available base-upgrade paths for a given level (2 or 3).
std::vector<const UpgradePathDef*> GetBaseUpgradesForLevel(int level);

/// Returns upgrade paths that apply to a specific building type.
std::vector<const UpgradePathDef*> GetBuildingUpgradesForType(BuildingType type);
