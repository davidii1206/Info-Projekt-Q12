/**
 * @file FogOfWar.h
 * @brief Grid-based Fog of War (Discover Map) system.
 *
 * The map is divided into a uniform grid of cells. Each cell has two states:
 *  - HIDDEN   – never visited, shown as dark overlay
 *  - REVEALED – a unit has been here, permanently visible
 *
 * Supports per-player (per-team) fog grids.
 */

#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include "Components.h"

// ---------------------------------------------------------------------------
// FogGrid – pure data, no ECS dependency
// ---------------------------------------------------------------------------

/**
 * @struct FogGrid
 * @brief Flat 2D bit-grid tracking which cells have been revealed.
 *
 * Also tracks dirty cells (newly revealed since last sync) for delta networking.
 */
struct FogGrid
{
    glm::vec3 worldMin{-50.f, 0.f, -50.f};
    glm::vec3 worldMax{ 50.f, 0.f,  50.f};
    float     cellSize = 1.f;

    int cellsX = 0;
    int cellsZ = 0;

    std::vector<bool> revealed;
    std::vector<bool> dirty;   ///< Newly revealed since last call to ConsumeDirty().

    void Init(glm::vec3 min = {-50.f,0.f,-50.f},
              glm::vec3 max = { 50.f,0.f, 50.f},
              float     cell = 1.f)
    {
        worldMin = min;
        worldMax = max;
        cellSize = cell;
        cellsX = static_cast<int>(std::ceil((max.x - min.x) / cell));
        cellsZ = static_cast<int>(std::ceil((max.z - min.z) / cell));
        size_t n = static_cast<size_t>(cellsX * cellsZ);
        revealed.assign(n, false);
        dirty.assign(n, false);
    }

    void WorldToCell(const glm::vec3& pos, int& outX, int& outZ) const
    {
        outX = static_cast<int>((pos.x - worldMin.x) / cellSize);
        outZ = static_cast<int>((pos.z - worldMin.z) / cellSize);
        outX = std::max(0, std::min(cellsX - 1, outX));
        outZ = std::max(0, std::min(cellsZ - 1, outZ));
    }

    bool InBounds(int cx, int cz) const
    {
        return cx >= 0 && cx < cellsX && cz >= 0 && cz < cellsZ;
    }

    bool Reveal(const glm::vec3& center, float radius)
    {
        int cx, cz;
        WorldToCell(center, cx, cz);
        bool changed = false;
        int cellRadius = static_cast<int>(std::ceil(radius / cellSize));
        for (int dz = -cellRadius; dz <= cellRadius; ++dz)
        for (int dx = -cellRadius; dx <= cellRadius; ++dx)
        {
            float wx = (cx + dx + 0.5f) * cellSize + worldMin.x;
            float wz = (cz + dz + 0.5f) * cellSize + worldMin.z;
            float dist2 = (wx - center.x) * (wx - center.x)
                        + (wz - center.z) * (wz - center.z);
            if (dist2 > radius * radius) continue;
            int nx = cx + dx;
            int nz = cz + dz;
            if (!InBounds(nx, nz)) continue;
            size_t idx = static_cast<size_t>(nz * cellsX + nx);
            if (!revealed[idx])
            {
                revealed[idx] = true;
                dirty[idx]    = true;
                changed = true;
            }
        }
        return changed;
    }

    bool IsRevealed(int cx, int cz) const
    {
        if (!InBounds(cx, cz)) return false;
        return revealed[static_cast<size_t>(cz * cellsX + cx)];
    }

    bool IsWorldPosRevealed(const glm::vec3& worldPos) const
    {
        int cx, cz;
        WorldToCell(worldPos, cx, cz);
        return IsRevealed(cx, cz);
    }

    void Reset()
    {
        revealed.assign(revealed.size(), false);
        dirty.assign(dirty.size(), false);
    }

    bool IsInitialised() const { return !revealed.empty(); }

    /// Returns a bit-packed vector of dirtied cells (row-major indices)
    /// and clears the dirty flags. The caller should send this delta to clients.
    void ConsumeDirty(std::vector<uint32_t>& outCells)
    {
        outCells.clear();
        size_t n = dirty.size();
        for (size_t i = 0; i < n; ++i)
            if (dirty[i]) outCells.push_back(static_cast<uint32_t>(i));
        dirty.assign(n, false);
    }

    /// Applies a batch of newly-revealed cell indices (from a delta packet).
    /// Returns true if at least one cell was newly revealed on this grid.
    bool ApplyDelta(const uint32_t* cells, size_t count)
    {
        bool changed = false;
        for (size_t i = 0; i < count; ++i)
        {
            size_t idx = cells[i];
            if (idx < revealed.size() && !revealed[idx])
            {
                revealed[idx] = true;
                changed = true;
            }
        }
        return changed;
    }
};

// ---------------------------------------------------------------------------
// FogOfWarSystem
// ---------------------------------------------------------------------------

namespace FogOfWarSystem
{
    constexpr float COMBAT_SIGHT_RADIUS    = 10.f;
    constexpr float COLLECTOR_SIGHT_RADIUS = 8.f;
    constexpr float UNIT_SIGHT_RADIUS      = 10.f;
    constexpr float BUILDING_SIGHT_RADIUS  = 8.f;

    /// Updates a per-team fog grid using only units belonging to `teamId`.
    /// Pass teamId=0xFFFFFFFF to skip team filtering (reveals everything).
    inline bool UpdateForTeam(FogGrid& fog, entt::registry& registry, uint32_t teamId)
    {
        if (!fog.IsInitialised()) return false;
        bool changed = false;

        auto revealIfTeam = [&](auto e, const glm::vec3& pos, float radius, bool skipTeamCheck) {
            auto* uc = registry.try_get<UnitComponent>(e);
            if (!skipTeamCheck)
            {
                if (!uc || uc->teamId != teamId) return;
            }
            float finalRadius = radius;
            if (uc && uc->bugClass == BugClass::ButterfliesMoths && uc->tier == 2) {
                finalRadius = 24.f; // Scout has massive vision
            }
            changed |= fog.Reveal(pos, finalRadius);
        };

        {
            auto view = registry.view<TransformComponent, CombatComponent>();
            for (auto e : view)
                revealIfTeam(e, view.get<TransformComponent>(e).position, COMBAT_SIGHT_RADIUS, teamId == 0xFFFFFFFF);
        }
        {
            auto view = registry.view<TransformComponent, CollectorComponent>();
            for (auto e : view)
                revealIfTeam(e, view.get<TransformComponent>(e).position, COLLECTOR_SIGHT_RADIUS, teamId == 0xFFFFFFFF);
        }
        {
            auto view = registry.view<TransformComponent, UnitComponent>();
            for (auto e : view)
            {
                if (registry.any_of<CombatComponent>(e) || registry.any_of<CollectorComponent>(e))
                    continue;
                revealIfTeam(e, view.get<TransformComponent>(e).position, UNIT_SIGHT_RADIUS, teamId == 0xFFFFFFFF);
            }
        }
        {
            auto view = registry.view<TransformComponent, BuildingComponent>();
            for (auto e : view)
            {
                auto& bc = view.get<BuildingComponent>(e);
                if (teamId != 0xFFFFFFFF && bc.teamId != teamId) continue;
                changed |= fog.Reveal(view.get<TransformComponent>(e).position, BUILDING_SIGHT_RADIUS);
            }
        }
        return changed;
    }

    /// Legacy: reveals fog for ALL units (no team filter).
    inline bool Update(FogGrid& fog, entt::registry& registry)
    {
        return UpdateForTeam(fog, registry, 0xFFFFFFFF);
    }

} // namespace FogOfWarSystem
