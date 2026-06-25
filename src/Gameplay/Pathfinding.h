#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "../Core/WorldManager.h"
#include "FogOfWar.h"

namespace Pathfinding {

    /// Maximum tiles A* will search before giving up.
    constexpr int MAX_SEARCH_TILES = 50000;

    /// Finds a path from start to end on the terrain tile grid.
    /// @param world  Terrain data (tile grid, tiers, surfaces).
    /// @param start  World XZ start position.
    /// @param end    World XZ destination.
    /// @param fog    Optional fog grid — undiscovered tiles are blocked.
    /// @param maxTierDiff  Maximum tier climb allowed per step (0 = same tier only).
    /// @param occupiedTiles  Optional per-tile bool array (gridSize², row-major).
    ///        Tiles marked true are treated as blocked (e.g. by buildings).
    /// @param isFlying  If true, the unit ignores terrain tier differences, water, and cliffs.
    /// @param isClimber If true, the unit can climb cliff faces directly
    ///                  (skips both the tier-difference and cliff-surface checks)
    ///                  but still respects water and occupied tiles.
    /// @return Waypoints in world XZ (tile centres), empty if no path found.
    std::vector<glm::vec2> FindPath(
        const WorldManager& world,
        glm::vec2 start,
        glm::vec2 end,
        const FogGrid* fog = nullptr,
        int maxTierDiff = 0,
        const std::vector<bool>* occupiedTiles = nullptr,
        bool isFlying = false,
        bool isClimber = false);

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
