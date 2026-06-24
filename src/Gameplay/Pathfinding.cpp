#include "Pathfinding.h"
#include <queue>
#include <cmath>
#include <algorithm>
#include <limits>
#include <spdlog/spdlog.h>

namespace Pathfinding {

// ---------------------------------------------------------------------------
// Helper data for the A* open set
// ---------------------------------------------------------------------------
struct AStarNode {
    int   tx, tz;
    float g, f;
};

struct CompareNode {
    bool operator()(const AStarNode& a, const AStarNode& b) const {
        return a.f > b.f; // min-heap
    }
};

// 4-directional neighbours (cardinal).
static const int kDX[] = { 1, -1,  0,  0 };
static const int kDZ[] = { 0,  0,  1, -1 };

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool IsTileWalkable(
    const WorldManager& world,
    int tx, int tz,
    int currentTier,
    int maxTierDiff,
    const FogGrid* fog)
{
    const int gs = world.GetGridSize();
    if (tx < 0 || tx >= gs || tz < 0 || tz >= gs) return false;

    const auto& tile = world.GetTile(tx, tz);

    // Reject water, cliff, boss arena
    if (tile.surface == TileSurface::Water)  return false;
    if (tile.surface == TileSurface::Cliff)  return false;

    // Reject tiles that are too high above or below the current tier.
    int tileTier = (int)tile.tier;
    if (std::abs(tileTier - currentTier) > maxTierDiff) return false;

    // If a fog grid is provided, only walk through revealed cells.
    if (fog && fog->IsInitialised()) {
        glm::vec2 wc = world.TileToWorld(tx, tz);
        if (!fog->IsWorldPosRevealed(glm::vec3(wc.x, 0.f, wc.y)))
            return false;
    }

    return true;
}

std::vector<glm::vec2> FindPath(
    const WorldManager& world,
    glm::vec2 start,
    glm::vec2 end,
    const FogGrid* fog,
    int maxTierDiff)
{
    const int gs = world.GetGridSize();
    if (gs <= 0) return {};

    // Convert start/end world positions to tile coordinates.
    int sx, sz, ex, ez;
    world.WorldToTile(start.x, start.y, sx, sz);
    world.WorldToTile(end.x,   end.y,   ex, ez);

    // Clamp destination to grid.
    ex = std::max(0, std::min(gs - 1, ex));
    ez = std::max(0, std::min(gs - 1, ez));

    // If start == end (same tile), return a single waypoint.
    if (sx == ex && sz == ez) {
        glm::vec2 wc = world.TileToWorld(ex, ez);
        return { wc };
    }

    // Determine the start tile's tier so the tier check is relative.
    int startTier = (int)world.GetTile(sx, sz).tier;

    // Check if the destination tile is itself walkable; if not, the path is
    // still allowed to end there (the unit will stop as close as it can).
    bool destWalkable = IsTileWalkable(world, ex, ez, startTier, maxTierDiff, fog);

    // Scratch grids – allocated once, reused across pathfinding calls.
    // visited: 0 = unvisited, 1 = open, 2 = closed
    // gScore & cameFrom are only valid when visited != 0.
    std::vector<uint8_t>  visited(static_cast<size_t>(gs) * gs, 0);
    std::vector<float>    gScore(static_cast<size_t>(gs) * gs, 0.f);
    std::vector<int>      cameFrom(static_cast<size_t>(gs) * gs, -1);

    auto idx = [gs](int tx, int tz) { return static_cast<size_t>(tz) * gs + tx; };

    // Priority queue (min-heap by f).
    std::priority_queue<AStarNode, std::vector<AStarNode>, CompareNode> open;

    // Heuristic: octile distance (accounts for diagonal movement if we ever
    // add 8-directional, but with 4-dir it's equivalent to Manhattan).
    auto heuristic = [](int ax, int az, int bx, int bz) -> float {
        float dx = (float)(bx - ax);
        float dz = (float)(bz - az);
        return std::sqrt(dx * dx + dz * dz);
    };

    float hStart = heuristic(sx, sz, ex, ez);
    open.push({ sx, sz, 0.f, hStart });
    visited[idx(sx, sz)] = 1;
    gScore[idx(sx, sz)]  = 0.f;
    cameFrom[idx(sx, sz)] = idx(sx, sz); // self = start marker

    int searched = 0;
    int goalIdx = idx(ex, ez);
    int bestIdx = idx(sx, sz);
    float bestHeuristic = hStart;

    while (!open.empty() && searched < MAX_SEARCH_TILES) {
        AStarNode cur = open.top();
        open.pop();

        int curIdx = idx(cur.tx, cur.tz);
        if (visited[curIdx] == 2) continue; // Already closed (stale entry).
        visited[curIdx] = 2;
        ++searched;

        // Reached the goal tile.
        if (cur.tx == ex && cur.tz == ez) {
            goalIdx = curIdx;
            break;
        }

        // Track closest tile to destination (fallback if no full path).
        float h = heuristic(cur.tx, cur.tz, ex, ez);
        if (h < bestHeuristic) {
            bestHeuristic = h;
            bestIdx = curIdx;
        }

        int curTier = (int)world.GetTile(cur.tx, cur.tz).tier;

        for (int d = 0; d < 4; ++d) {
            int nx = cur.tx + kDX[d];
            int nz = cur.tz + kDZ[d];

            if (nx < 0 || nx >= gs || nz < 0 || nz >= gs) continue;

            size_t ni = idx(nx, nz);
            if (visited[ni] == 2) continue;

            if (!IsTileWalkable(world, nx, nz, curTier, maxTierDiff, fog)) {
                // Mark as closed so we don't re-evaluate every frame.
                // But only if it's not the destination tile.
                if (!(nx == ex && nz == ez) || !destWalkable) {
                    visited[ni] = 2;
                    continue;
                }
            }

            float stepCost = 1.f; // uniform cost per tile
            float ng = cur.g + stepCost;
            float nh = heuristic(nx, nz, ex, ez);

            if (visited[ni] != 1 || ng < gScore[ni]) {
                visited[ni] = 1;
                gScore[ni]  = ng;
                cameFrom[ni] = curIdx;
                open.push({ nx, nz, ng, ng + nh });
            }
        }
    }

    if (searched >= MAX_SEARCH_TILES) {
        spdlog::warn("Pathfinding: search limit reached ({} tiles)", MAX_SEARCH_TILES);
    }

    // Reconstruct path from goal (or best).
    int traceIdx = (visited[goalIdx] != 0) ? goalIdx : bestIdx;
    if (cameFrom[traceIdx] < 0) {
        // No path at all — return empty.
        return {};
    }

    std::vector<glm::vec2> waypoints;
    while (true) {
        int tx = traceIdx % gs;
        int tz = traceIdx / gs;
        waypoints.push_back(world.TileToWorld(tx, tz));

        if (traceIdx == cameFrom[traceIdx]) break; // reached start
        traceIdx = cameFrom[traceIdx];
    }

    std::reverse(waypoints.begin(), waypoints.end());
    return waypoints;
}

} // namespace Pathfinding
