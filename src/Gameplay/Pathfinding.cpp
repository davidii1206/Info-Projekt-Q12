#include "Pathfinding.h"
#include <queue>
#include <cmath>
#include <algorithm>
#include <limits>
#include <spdlog/spdlog.h>

namespace Pathfinding {

// ---------------------------------------------------------------------------
// Neighbour directions: 4 cardinal + 4 diagonal (8-directional movement).
// Diagonal step cost = sqrt(2) ≈ 1.41421.
// ---------------------------------------------------------------------------
static const int   kDX[]    = { 1, -1,  0,  0,  1, -1,  1, -1 };
static const int   kDZ[]    = { 0,  0,  1, -1,  1,  1, -1, -1 };
static const float kDCost[] = { 1.f, 1.f, 1.f, 1.f,
                                 1.41421356f, 1.41421356f, 1.41421356f, 1.41421356f };
static constexpr int kNumDirs = 8;

// ---------------------------------------------------------------------------
// A* open-set node
// ---------------------------------------------------------------------------
struct AStarNode {
    int   tx, tz;
    float g, f;
};

struct CompareNode {
    bool operator()(const AStarNode& a, const AStarNode& b) const {
        return a.f > b.f; // min-heap by f
    }
};

// ---------------------------------------------------------------------------
// Octile-distance heuristic — admissible & consistent for 8-directional
// movement where diagonals cost sqrt(2).
// ---------------------------------------------------------------------------
static inline float Heuristic(int ax, int az, int bx, int bz)
{
    float dx = std::abs(static_cast<float>(bx - ax));
    float dz = std::abs(static_cast<float>(bz - az));
    return (dx + dz) + (1.41421356f - 2.f) * std::min(dx, dz);
}

// ---------------------------------------------------------------------------
// Reduce a path: skip waypoints that are collinear with their neighbours.
// ---------------------------------------------------------------------------
static std::vector<glm::vec2> SimplifyPath(const std::vector<glm::vec2>& pts)
{
    const size_t n = pts.size();
    if (n <= 2) return pts;

    std::vector<glm::vec2> out;
    out.reserve(n / 4 + 2);
    out.push_back(pts[0]);

    for (size_t i = 1; i + 1 < n; ++i) {
        glm::vec2 d1 = pts[i]     - pts[i - 1];
        glm::vec2 d2 = pts[i + 1] - pts[i];
        // Cross product ≈ 0 → collinear → skip middle point.
        float cross = d1.x * d2.y - d1.y * d2.x;
        if (std::abs(cross) > 0.01f)
            out.push_back(pts[i]);
    }

    out.push_back(pts[n - 1]);
    return out;
}

// ---------------------------------------------------------------------------
// IsTileWalkable — still accepts fog for external callers that need it.
// FindPath itself does NOT use fog (fog = visibility, not traversability).
// ---------------------------------------------------------------------------
bool IsTileWalkable(
    const WorldManager& world,
    int tx, int tz,
    int currentTier,
    int maxTierDiff,
    const FogGrid* fog,
    const std::vector<bool>* occupiedTiles,
    bool isFlying,
    bool isClimber)
{
    const int gs = world.GetGridSize();
    if (tx < 0 || tx >= gs || tz < 0 || tz >= gs) return false;

    if (!isFlying) {
        if (occupiedTiles) {
            size_t i = static_cast<size_t>(tz) * gs + tx;
            if (i < occupiedTiles->size() && (*occupiedTiles)[i])
                return false;
        }

        const auto& tile = world.GetTile(tx, tz);

        if (tile.surface == TileSurface::Water) return false;

        if (!isClimber) {
            if (tile.surface == TileSurface::Cliff) return false;
            if (std::abs(static_cast<int>(tile.tier) - currentTier) > maxTierDiff)
                return false;
        }
    }

    if (fog && fog->IsInitialised()) {
        glm::vec2 wc = world.TileToWorld(tx, tz);
        if (!fog->IsWorldPosRevealed(glm::vec3(wc.x, 0.f, wc.y)))
            return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// FindPath — 8-directional A* without fog constraint.
// Fog is a visibility concept; units may walk through uncharted territory.
// ---------------------------------------------------------------------------
PathResult FindPath(
    const WorldManager& world,
    glm::vec2 start,
    glm::vec2 end,
    int maxTierDiff,
    const std::vector<bool>* occupiedTiles,
    bool isFlying,
    bool isClimber,
    bool warnOnLimit)
{
    const int gs = world.GetGridSize();
    if (gs <= 0) return {};

    // Dynamic budget: allow searching the full grid so terrain detours (long
    // cliff corridors, water wraparounds) don't produce false partial paths.
    // On a 375×375 map this is ~140k tiles — A* with octile heuristic will
    // finish in far fewer for any reachable destination.
    const int kSearchBudget = gs * gs;

    int sx, sz, ex, ez;
    world.WorldToTile(start.x, start.y, sx, sz);
    world.WorldToTile(end.x,   end.y,   ex, ez);

    ex = std::max(0, std::min(gs - 1, ex));
    ez = std::max(0, std::min(gs - 1, ez));

    if (sx == ex && sz == ez)
        return { { world.TileToWorld(ex, ez) }, true };

    const int startTier = static_cast<int>(world.GetTile(sx, sz).tier);
    bool destWalkable = IsTileWalkable(world, ex, ez, startTier, maxTierDiff,
                                        nullptr, occupiedTiles, isFlying, isClimber);

    const size_t cells = static_cast<size_t>(gs) * gs;

    // visited: 0=unvisited, 1=open, 2=closed
    std::vector<uint8_t> visited(cells, 0);
    std::vector<float>   gScore(cells, std::numeric_limits<float>::max());
    std::vector<int>     cameFrom(cells, -1);

    auto cellIdx = [gs](int tx, int tz) -> size_t {
        return static_cast<size_t>(tz) * gs + tx;
    };

    std::priority_queue<AStarNode, std::vector<AStarNode>, CompareNode> open;

    const size_t startI = cellIdx(sx, sz);
    float hStart = Heuristic(sx, sz, ex, ez);
    open.push({ sx, sz, 0.f, hStart });
    visited[startI] = 1;
    gScore[startI]  = 0.f;
    cameFrom[startI] = static_cast<int>(startI); // self = start marker

    int    searched  = 0;
    size_t goalIdx   = cellIdx(ex, ez);
    size_t bestIdx   = startI;
    float  bestH     = hStart;
    bool   goalFound = false;

    while (!open.empty() && searched < kSearchBudget) {
        AStarNode cur = open.top();
        open.pop();

        size_t curIdx = cellIdx(cur.tx, cur.tz);
        if (visited[curIdx] == 2) continue; // stale entry
        visited[curIdx] = 2;
        ++searched;

        if (cur.tx == ex && cur.tz == ez) {
            goalIdx   = curIdx;
            goalFound = true;
            break;
        }

        float h = Heuristic(cur.tx, cur.tz, ex, ez);
        if (h < bestH) {
            bestH   = h;
            bestIdx = curIdx;
        }

        int curTier = static_cast<int>(world.GetTile(cur.tx, cur.tz).tier);

        for (int d = 0; d < kNumDirs; ++d) {
            int nx = cur.tx + kDX[d];
            int nz = cur.tz + kDZ[d];

            if (nx < 0 || nx >= gs || nz < 0 || nz >= gs) continue;
            size_t ni = cellIdx(nx, nz);
            if (visited[ni] == 2) continue;

            // Diagonal corner-cut prevention: both cardinal neighbours must be
            // passable so units don't squeeze through obstacle corners.
            if (d >= 4) {
                if (!IsTileWalkable(world, cur.tx + kDX[d], cur.tz, curTier, maxTierDiff,
                                    nullptr, occupiedTiles, isFlying, isClimber) ||
                    !IsTileWalkable(world, cur.tx, cur.tz + kDZ[d], curTier, maxTierDiff,
                                    nullptr, occupiedTiles, isFlying, isClimber))
                    continue;
            }

            bool walkable = IsTileWalkable(world, nx, nz, curTier, maxTierDiff,
                                            nullptr, occupiedTiles, isFlying, isClimber);
            if (!walkable) {
                bool isGoal = (nx == ex && nz == ez);
                if (!isGoal || !destWalkable) {
                    visited[ni] = 2;
                    continue;
                }
            }

            float ng = cur.g + kDCost[d];
            if (ng < gScore[ni]) {
                gScore[ni]   = ng;
                visited[ni]  = 1;
                cameFrom[ni] = static_cast<int>(curIdx);
                open.push({ nx, nz, ng, ng + Heuristic(nx, nz, ex, ez) });
            }
        }
    }

    if (searched >= kSearchBudget && !goalFound && warnOnLimit) {
        spdlog::debug("Pathfinding: full grid searched ({} tiles) — destination unreachable, using closest tile",
                      searched);
    }

    // Reconstruct from goal (full path) or best tile (partial path).
    size_t traceIdx = goalFound ? goalIdx
                                : (visited[goalIdx] != 0 ? goalIdx : bestIdx);
    if (cameFrom[traceIdx] < 0)
        return {};

    std::vector<glm::vec2> waypoints;
    waypoints.reserve(64);
    while (true) {
        int tx = static_cast<int>(traceIdx) % gs;
        int tz = static_cast<int>(traceIdx) / gs;
        waypoints.push_back(world.TileToWorld(tx, tz));

        if (static_cast<size_t>(cameFrom[traceIdx]) == traceIdx) break; // start
        traceIdx = static_cast<size_t>(cameFrom[traceIdx]);
    }

    std::reverse(waypoints.begin(), waypoints.end());
    return { SimplifyPath(waypoints), goalFound };
}

} // namespace Pathfinding
