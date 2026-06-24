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
    /// @return Waypoints in world XZ (tile centres), empty if no path found.
    std::vector<glm::vec2> FindPath(
        const WorldManager& world,
        glm::vec2 start,
        glm::vec2 end,
        const FogGrid* fog = nullptr,
        int maxTierDiff = 0);

    /// Checks whether a tile at (tx, tz) is walkable.
    bool IsTileWalkable(
        const WorldManager& world,
        int tx, int tz,
        int currentTier,
        int maxTierDiff = 0,
        const FogGrid* fog = nullptr);

} // namespace Pathfinding
