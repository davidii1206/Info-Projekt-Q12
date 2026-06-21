#include "UpgradeSystem.h"
#include "Components.h"
#include <spdlog/spdlog.h>
#include <unordered_map>

namespace UpgradeSystem {

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static std::unordered_map<uint32_t, TeamState> s_Teams;

void Reset() {
    s_Teams.clear();
}

TeamState& GetState(uint32_t teamId) {
    return s_Teams[teamId];
}

void AddKill(uint32_t teamId) {
    s_Teams[teamId].killCount++;
}

bool HasCompleted(uint32_t teamId, UpgradePathID pathId) {
    auto it = s_Teams.find(teamId);
    if (it == s_Teams.end()) return false;
    for (auto id : it->second.completedPaths)
        if (id == pathId) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Requirement checking
// ---------------------------------------------------------------------------
static bool CheckRequirement(entt::registry& registry, uint32_t teamId,
                             const UpgradeRequirementDef& req) {
    switch (req.type) {
    case UpgradeReqType::Resources: {
        // Find the team's stockpile (Base or Main/Storage with ResourceInventory)
        auto baseView = registry.view<BaseComponent, ResourceInventory>();
        for (auto e : baseView) {
            if (baseView.get<BaseComponent>(e).teamId == teamId) {
                auto& inv = baseView.get<ResourceInventory>(e);
                return inv.pilze    >= req.cost.pilze    &&
                       inv.beeren   >= req.cost.beeren   &&
                       inv.nektar   >= req.cost.nektar   &&
                       inv.samen    >= req.cost.samen    &&
                       inv.insekten >= req.cost.insekten &&
                       inv.fleisch  >= req.cost.fleisch;
            }
        }
        // Fallback: check buildings with ResourceInventory
        auto bldView = registry.view<BuildingComponent, ResourceInventory>();
        for (auto e : bldView) {
            auto& bc = bldView.get<BuildingComponent>(e);
            if (bc.teamId == teamId && !bc.destroyed) {
                auto& inv = bldView.get<ResourceInventory>(e);
                if (inv.pilze    >= req.cost.pilze    &&
                    inv.beeren   >= req.cost.beeren   &&
                    inv.nektar   >= req.cost.nektar   &&
                    inv.samen    >= req.cost.samen    &&
                    inv.insekten >= req.cost.insekten &&
                    inv.fleisch  >= req.cost.fleisch)
                    return true;
            }
        }
        return false;
    }
    case UpgradeReqType::KillCount: {
        auto it = s_Teams.find(teamId);
        return it != s_Teams.end() && it->second.killCount >= req.value;
    }
    case UpgradeReqType::BuildingCount: {
        int count = 0;
        auto view = registry.view<BuildingComponent>();
        for (auto e : view) {
            auto& bc = view.get<BuildingComponent>(e);
            if (bc.teamId == teamId && !bc.destroyed &&
                bc.type == req.targetBuilding)
                ++count;
        }
        return count >= req.value;
    }
    case UpgradeReqType::HasUpgrade:
        return HasCompleted(teamId, req.prerequisite);
    case UpgradeReqType::TimeElapsed:
        // The caller must pass total time via registry or a ctx variable.
        // For now we always return true so this requirement type is always met
        // unless game time tracking is wired in.
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Resource deduction
// ---------------------------------------------------------------------------
static bool DeductResources(entt::registry& registry, uint32_t teamId,
                            const ResourceInventory& cost) {
    auto baseView = registry.view<BaseComponent, ResourceInventory>();
    for (auto e : baseView) {
        auto& bc = baseView.get<BaseComponent>(e);
        if (bc.teamId == teamId) {
            auto& inv = baseView.get<ResourceInventory>(e);
            inv.pilze    -= cost.pilze;
            inv.beeren   -= cost.beeren;
            inv.nektar   -= cost.nektar;
            inv.samen    -= cost.samen;
            inv.insekten -= cost.insekten;
            inv.fleisch  -= cost.fleisch;
            return true;
        }
    }
    // Fallback: deduct from the first non-destroyed building inventory
    auto bldView = registry.view<BuildingComponent, ResourceInventory>();
    for (auto e : bldView) {
        auto& bc = bldView.get<BuildingComponent>(e);
        if (bc.teamId == teamId && !bc.destroyed) {
            auto& inv = bldView.get<ResourceInventory>(e);
            inv.pilze    -= cost.pilze;
            inv.beeren   -= cost.beeren;
            inv.nektar   -= cost.nektar;
            inv.samen    -= cost.samen;
            inv.insekten -= cost.insekten;
            inv.fleisch  -= cost.fleisch;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool CanApply(entt::registry& registry, uint32_t teamId, UpgradePathID pathId) {
    auto* def = FindUpgradeDef(pathId);
    if (!def) return false;

    auto& state = GetState(teamId);

    // Check base level prerequisites
    if (def->appliesTo == BuildingType::Count || def->appliesTo == BuildingType::Main) {
        // Base upgrade: check level
        int requiredLevel = 0;
        if (def->id <= 99) { // Heuristic: base upgrades are <100
            // Level 2 paths: 1-3, Level 3 paths: 4-6
            if (def->id <= 3) requiredLevel = 1;
            else requiredLevel = 2;
        } else {
            // For building upgrades, we need the base at level 2+
            requiredLevel = 2;
        }
        if (state.baseLevel < requiredLevel) return false;
    }

    // Check all requirements
    for (const auto& req : def->requirements) {
        if (!CheckRequirement(registry, teamId, req))
            return false;
    }
    return true;
}

bool Apply(entt::registry& registry, uint32_t teamId, UpgradePathID pathId) {
    auto* def = FindUpgradeDef(pathId);
    if (!def) return false;

    if (!CanApply(registry, teamId, pathId)) return false;

    auto& state = GetState(teamId);

    // Deduct resource costs
    for (const auto& req : def->requirements) {
        if (req.type == UpgradeReqType::Resources) {
            if (!DeductResources(registry, teamId, req.cost))
                return false;
        }
    }

    state.completedPaths.push_back(pathId);

    // If it's a main base upgrade, increase base level
    if (def->appliesTo == BuildingType::Count || def->appliesTo == BuildingType::Main) {
        if (pathId <= 3) state.baseLevel = 2;
        else if (pathId <= 6) state.baseLevel = 3;
    }

    spdlog::info("[UpgradeSystem] Team {} completed upgrade '{}' (id={})",
                 teamId, def->name, pathId);
    return true;
}

// ---------------------------------------------------------------------------
// Passive-bonus helpers
// ---------------------------------------------------------------------------
static float AccumMul(uint32_t teamId, float UpgradeEffectDef::*field) {
    auto it = s_Teams.find(teamId);
    if (it == s_Teams.end()) return 1.0f;
    float mul = 1.0f;
    for (auto pid : it->second.completedPaths) {
        auto* def = FindUpgradeDef(pid);
        if (def) mul *= def->effects.*field;
    }
    return mul;
}

float GetUnitDamageMul   (uint32_t teamId) { return AccumMul(teamId, &UpgradeEffectDef::unitDamageMul); }
float GetUnitDefenseMul  (uint32_t teamId) { return AccumMul(teamId, &UpgradeEffectDef::unitDefenseMul); }
float GetUnitSpeedMul    (uint32_t teamId) { return AccumMul(teamId, &UpgradeEffectDef::unitSpeedMul); }
float GetBuildingHpMul   (uint32_t teamId) { return AccumMul(teamId, &UpgradeEffectDef::buildingHpMul); }
float GetBuildingCostMul (uint32_t teamId) { return AccumMul(teamId, &UpgradeEffectDef::buildingCostMul); }
float GetGatherRateMul   (uint32_t teamId) { return AccumMul(teamId, &UpgradeEffectDef::resourceGatherRateMul); }
float GetStorageMul      (uint32_t teamId) { return AccumMul(teamId, &UpgradeEffectDef::resourceStorageMul); }
float GetSpawnRateMul    (uint32_t teamId) { return AccumMul(teamId, &UpgradeEffectDef::unitSpawnRateMul); }
float GetConversionRateMul(uint32_t teamId){ return AccumMul(teamId, &UpgradeEffectDef::conversionRateMul); }

bool IsBuildingUnlocked(uint32_t teamId, BuildingType type) {
    auto it = s_Teams.find(teamId);
    if (it == s_Teams.end()) return false;
    for (auto pid : it->second.completedPaths) {
        auto* def = FindUpgradeDef(pid);
        if (def && def->effects.unlockBuilding == type)
            return true;
    }
    // Main and Storage are always unlocked
    return (type == BuildingType::Main || type == BuildingType::Storage);
}

bool IsSpecialUnlocked(uint32_t teamId, SpecialBuildingType type) {
    auto it = s_Teams.find(teamId);
    if (it == s_Teams.end()) return false;
    for (auto pid : it->second.completedPaths) {
        auto* def = FindUpgradeDef(pid);
        if (def && def->effects.unlockSpecial == type)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Available upgrade queries
// ---------------------------------------------------------------------------

std::vector<UpgradePathID> AvailableBaseUpgrades(entt::registry& registry, uint32_t teamId) {
    auto& state = GetState(teamId);
    std::vector<UpgradePathID> result;

    if (state.baseLevel >= 3) return result; // Max level reached

    int targetLevel = state.baseLevel + 1; // 2 or 3
    auto paths = GetBaseUpgradesForLevel(targetLevel);

    for (auto* def : paths) {
        if (!def) continue;
        // Skip if already completed
        if (HasCompleted(teamId, def->id)) continue;
        // Check requirements
        bool ok = true;
        for (const auto& req : def->requirements) {
            if (!CheckRequirement(registry, teamId, req)) { ok = false; break; }
        }
        if (ok) result.push_back(def->id);
    }
    return result;
}

std::vector<UpgradePathID> AvailableBuildingUpgrades(entt::registry& registry,
                                                     uint32_t teamId,
                                                     BuildingType bldType) {
    auto paths = GetBuildingUpgradesForType(bldType);
    std::vector<UpgradePathID> result;

    for (auto* def : paths) {
        if (!def) continue;
        if (HasCompleted(teamId, def->id)) continue;
        bool ok = true;
        for (const auto& req : def->requirements) {
            if (!CheckRequirement(registry, teamId, req)) { ok = false; break; }
        }
        if (ok) result.push_back(def->id);
    }
    return result;
}

} // namespace UpgradeSystem
