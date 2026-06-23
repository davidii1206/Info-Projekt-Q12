/**
 * @file FogOfWar.h
 * @brief Grid-based Fog of War (Discover Map) system.
 *
 * The map is divided into a uniform grid of cells. Each cell has two states:
 *  - HIDDEN   – never visited, shown as dark overlay
 *  - REVEALED – a unit has been here, permanently visible
 *
 * The FogGrid is a plain value object that can live on the server and be
 * serialised to clients (or kept locally for a single-player / host setup).
 *
 * Usage (server, inside GameScene):
 * @code
 *   // Init once in OnEnter()
 *   m_Fog.Init(worldMin, worldMax, cellSize);
 *
 *   // Call every fixed tick in FixedUpdate()
 *   FogOfWarSystem::Update(m_Fog, ctx.serverRegistry);
 * @endcode
 *
 * Usage (client UI, inside UIUpdate()):
 * @code
 *   FogOfWarSystem::DrawOverlay(m_Fog, mapOriginScreen, mapSizeScreen);
 * @endcode
 */

#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
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
 * Coordinates: X = right, Z = depth (Y is height, ignored for fog purposes).
 */
struct FogGrid
{
    glm::vec3 worldMin{-50.f, 0.f, -50.f}; ///< Bottom-left corner of the grid.
    glm::vec3 worldMax{ 50.f, 0.f,  50.f}; ///< Top-right corner of the grid.
    float     cellSize = 2.f;               ///< World-units per cell edge.

    int cellsX = 0; ///< Number of cells along X.
    int cellsZ = 0; ///< Number of cells along Z.

    /// Flat array: revealed[z * cellsX + x] == true → cell visible.
    std::vector<bool> revealed;

    /**
     * @brief Initialises the grid. Call once before use.
     * @param min      World-space minimum (X and Z used).
     * @param max      World-space maximum (X and Z used).
     * @param cell     World-units per cell edge.
     */
    void Init(glm::vec3 min = {-50.f,0.f,-50.f},
              glm::vec3 max = { 50.f,0.f, 50.f},
              float     cell = 2.f)
    {
        worldMin = min;
        worldMax = max;
        cellSize = cell;

        cellsX = static_cast<int>(std::ceil((max.x - min.x) / cell));
        cellsZ = static_cast<int>(std::ceil((max.z - min.z) / cell));

        revealed.assign(static_cast<size_t>(cellsX * cellsZ), false);
    }

    /// Converts a world position to a grid cell index (clamped).
    void WorldToCell(const glm::vec3& pos, int& outX, int& outZ) const
    {
        outX = static_cast<int>((pos.x - worldMin.x) / cellSize);
        outZ = static_cast<int>((pos.z - worldMin.z) / cellSize);
        outX = std::max(0, std::min(cellsX - 1, outX));
        outZ = std::max(0, std::min(cellsZ - 1, outZ));
    }

    /// Returns true if the given grid index is inside the grid bounds.
    bool InBounds(int cx, int cz) const
    {
        return cx >= 0 && cx < cellsX && cz >= 0 && cz < cellsZ;
    }

    /// Reveals all cells within `radius` world-units of `center`.
    /// Returns true if at least one cell was newly revealed.
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
            if (InBounds(nx, nz) && !revealed[static_cast<size_t>(nz * cellsX + nx)])
            {
                revealed[static_cast<size_t>(nz * cellsX + nx)] = true;
                changed = true;
            }
        }
        return changed;
    }

    /// @return True if the cell (cx, cz) has been revealed.
    bool IsRevealed(int cx, int cz) const
    {
        if (!InBounds(cx, cz)) return false;
        return revealed[static_cast<size_t>(cz * cellsX + cx)];
    }

    /// @return True if the world position is in a revealed cell.
    bool IsWorldPosRevealed(const glm::vec3& worldPos) const
    {
        int cx, cz;
        WorldToCell(worldPos, cx, cz);
        return IsRevealed(cx, cz);
    }

    /// Resets all cells to hidden (useful on map restart).
    void Reset() { revealed.assign(revealed.size(), false); }

    /// @return True once Init() has allocated the grid.
    bool IsInitialised() const { return !revealed.empty(); }
};

// ---------------------------------------------------------------------------
// FogOfWarSystem
// ---------------------------------------------------------------------------

/**
 * @namespace FogOfWarSystem
 * @brief ECS system that updates the FogGrid based on unit positions.
 */
namespace FogOfWarSystem
{
    /// Sichtweite für Kampfeinheiten (Welt-Einheiten).
    constexpr float COMBAT_SIGHT_RADIUS    = 10.f;
    /// Sichtweite für Sammel- und AI-Einheiten.
    constexpr float COLLECTOR_SIGHT_RADIUS = 8.f;
    /// Sichtweite für sonstige Einheiten.
    constexpr float UNIT_SIGHT_RADIUS      = 10.f;
    /// Sichtweite für Gebäude (Bauauslösung).
    constexpr float BUILDING_SIGHT_RADIUS  = 8.f;

    /**
     * @brief Reveals fog around every unit that has a TransformComponent.
     *
     * CombatComponent units use COMBAT_SIGHT_RADIUS, CollectorComponent units use
     * COLLECTOR_SIGHT_RADIUS, other UnitComponent entities use UNIT_SIGHT_RADIUS,
     * and buildings use BUILDING_SIGHT_RADIUS.
     *
     * Call every server fixed-tick.
     *
     * @param fog      The grid to update.
     * @param registry Server or client registry.
     */
    inline bool Update(FogGrid& fog, entt::registry& registry)
    {
        if (!fog.IsInitialised()) return false;

        bool changed = false;

        {
            auto view = registry.view<TransformComponent, CombatComponent>();
            for (auto e : view)
                changed |= fog.Reveal(view.get<TransformComponent>(e).position,
                                     COMBAT_SIGHT_RADIUS);
        }

        {
            auto view = registry.view<TransformComponent, CollectorComponent>();
            for (auto e : view)
                changed |= fog.Reveal(view.get<TransformComponent>(e).position,
                                     COLLECTOR_SIGHT_RADIUS);
        }

        {
            auto view = registry.view<TransformComponent, UnitComponent>();
            for (auto e : view) {
                if (registry.any_of<CombatComponent>(e) || registry.any_of<CollectorComponent>(e))
                    continue;
                changed |= fog.Reveal(view.get<TransformComponent>(e).position,
                                     UNIT_SIGHT_RADIUS);
            }
        }

        {
            auto view = registry.view<TransformComponent, BuildingComponent>();
            for (auto e : view)
                changed |= fog.Reveal(view.get<TransformComponent>(e).position,
                                     BUILDING_SIGHT_RADIUS);
        }

        return changed;
    }

} // namespace FogOfWarSystem
