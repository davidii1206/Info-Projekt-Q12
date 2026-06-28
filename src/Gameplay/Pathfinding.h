#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "../Core/WorldManager.h"
#include "FogOfWar.h"

namespace Pathfinding {

    /// Maximum tiles A* will search — scales with grid size at call time.
    /// Use the inline helper below; this constant is the per-tile fallback cap.
    constexpr int MAX_SEARCH_TILES = 50000; // kept for ABI, prefer dynamic budget

    /// Result of a FindPath call.
    struct PathResult {
        std::vector<glm::vec2> waypoints; ///< World XZ tile centres (empty = no path).
        bool goalReached = false;          ///< True only when A* reached the exact destination.
    };

    /// Finds a path from start to end on the terrain tile grid.
    /// Fog-of-war is intentionally NOT a constraint — fog affects visibility,
    /// not terrain traversability. Only hard terrain (water, cliffs, buildings)
    /// and tier differences block movement.
    /// @param world         Terrain data (tile grid, tiers, surfaces).
    /// @param start         World XZ start position.
    /// @param end           World XZ destination.
    /// @param maxTierDiff   Maximum tier climb allowed per step (1 = one tier at a time).
    /// @param occupiedTiles Optional per-tile bool array (gridSize², row-major).
    ///                      Tiles marked true are treated as blocked (buildings).
    /// @param isFlying      Ignores terrain tier differences, water, and cliffs.
    /// @param isClimber     Can climb cliff faces (skips cliff and tier-diff checks).
    /// @return PathResult with waypoints and a flag indicating whether the goal was reached.
    /// @param warnOnLimit  Emit a log message when the search budget is exhausted.
    ///                     Pass false for periodic recalcs to avoid repeated spam.
    PathResult FindPath(
        const WorldManager& world,
        glm::vec2 start,
        glm::vec2 end,
        int maxTierDiff = 1,
        const std::vector<bool>* occupiedTiles = nullptr,
        bool isFlying = false,
        bool isClimber = false,
        bool warnOnLimit = true);

    /// Checks whether a tile at (tx, tz) is walkable.
    bool IsTileWalkable(
        const WorldManager& world,
        int tx, int tz,
        int currentTier,
        int maxTierDiff = 0,
        const FogGrid* fog = nullptr,
        const std::vector<bool>* occupiedTiles = nullptr,
        bool isFlying = false,
        bool isClimber = false);

} // namespace Pathfinding
