#pragma once
#include <entt/entt.hpp>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "UpgradeDefs.h"

namespace UpgradeSystem {

    /// Per-team upgrade state.
    struct TeamState {
        int baseLevel = 1;                          ///< Current base level (gates base upgrades).
        std::vector<UpgradePathID> completedPaths;  ///< IDs of completed upgrade paths.
        int killCount = 0;                          ///< Enemy kills (used by KillCount requirements).
    };

    /// Initialise / reset all team state.
    void Reset();

    /// Get (or create) the state for a given team.
    TeamState& GetState(uint32_t teamId);

    /// Record a kill for a team.
    void AddKill(uint32_t teamId);

    /// Check whether a team can afford / is eligible for an upgrade path.
    bool CanApply(entt::registry& registry, uint32_t teamId, UpgradePathID pathId);

    /// Apply an upgrade: deduct resources, record completion, return true on success.
    bool Apply(entt::registry& registry, uint32_t teamId, UpgradePathID pathId);

    /// Whether a team has completed a specific upgrade path.
    bool HasCompleted(uint32_t teamId, UpgradePathID pathId);

    // ------------------------------------------------------------------
    // Passive-bonus queries – each returns the product of all matching
    // completed upgrade effects for the given team.
    // ------------------------------------------------------------------
    float GetUnitDamageMul   (uint32_t teamId);
    float GetUnitDefenseMul  (uint32_t teamId);
    float GetUnitSpeedMul    (uint32_t teamId);
    float GetBuildingHpMul   (uint32_t teamId);
    float GetBuildingCostMul (uint32_t teamId);
    float GetGatherRateMul   (uint32_t teamId);
    float GetStorageMul      (uint32_t teamId);
    float GetSpawnRateMul    (uint32_t teamId);
    float GetConversionRateMul(uint32_t teamId);

    /// Whether a given BuildingType has been unlocked by any upgrade.
    bool IsBuildingUnlocked(uint32_t teamId, BuildingType type);
    bool IsSpecialUnlocked (uint32_t teamId, SpecialBuildingType type);

    /// Return the IDs of currently available base-upgrade paths for a team.
    std::vector<UpgradePathID> AvailableBaseUpgrades(entt::registry& registry, uint32_t teamId);

    /// Return the IDs of currently available upgrades for a specific building entity.
    std::vector<UpgradePathID> AvailableBuildingUpgrades(entt::registry& registry,
                                                         uint32_t teamId,
                                                         BuildingType bldType);

} // namespace UpgradeSystem
