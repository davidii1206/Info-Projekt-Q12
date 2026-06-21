#include "WorldManager.h"
#include "../Graphics/API/Texture.h"
#include <random>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

#define JC_VORONOI_IMPLEMENTATION
#include <jc_voronoi.h>

namespace {
    constexpr int kDX[4] = { 1, -1, 0, 0 };
    constexpr int kDZ[4] = { 0, 0, 1, -1 };
}

WorldManager::WorldManager() {}
WorldManager::~WorldManager() { Clear(); }

void WorldManager::Clear() {
    m_Terrains.clear();
    m_Tiles.clear();
    m_GridSize = 0;
}

void WorldManager::WorldToTile(float wx, float wz, int& outTx, int& outTz) const {
    const float ext = m_CurrentConfig.worldExtent;
    const float ts  = m_CurrentConfig.tileSize;
    outTx = (int)std::floor((wx + ext) / ts);
    outTz = (int)std::floor((wz + ext) / ts);
    outTx = std::clamp(outTx, 0, m_GridSize - 1);
    outTz = std::clamp(outTz, 0, m_GridSize - 1);
}

glm::vec2 WorldManager::TileToWorld(int tx, int tz) const {
    const float ext = m_CurrentConfig.worldExtent;
    const float ts  = m_CurrentConfig.tileSize;
    return glm::vec2(-ext + ((float)tx + 0.5f) * ts, -ext + ((float)tz + 0.5f) * ts);
}

const TerrainTile& WorldManager::GetTile(int tx, int tz) const {
    tx = std::clamp(tx, 0, m_GridSize - 1);
    tz = std::clamp(tz, 0, m_GridSize - 1);
    return m_Tiles[(size_t)tz * m_GridSize + tx];
}

void WorldManager::Generate(const WorldGenConfig& config) {
    m_CurrentConfig = config;
    Clear();

    // -----------------------------------------------------------------------
    // Territory layout: Voronoi + Lloyd relaxation. (KEEP — see WORLDGEN_PLAN)
    // -----------------------------------------------------------------------
    std::mt19937 rng(config.seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    std::vector<jcv_point> points(config.numTerrains);
    for (int i = 0; i < config.numTerrains; ++i) {
        points[i].x = dist(rng) * config.width;
        points[i].y = dist(rng) * config.height;
    }

    jcv_rect rect = { {0, 0}, {(float)config.width, (float)config.height} };
    for (int it = 0; it < config.relaxationIterations; ++it) {
        jcv_diagram diagram;
        memset(&diagram, 0, sizeof(jcv_diagram));
        jcv_diagram_generate(points.size(), points.data(), &rect, nullptr, &diagram);
        const jcv_site* sites = jcv_diagram_get_sites(&diagram);
        for (int i = 0; i < diagram.numsites; ++i) {
            const jcv_site* site = &sites[i];
            jcv_point center = { 0, 0 };
            int count = 0;
            const jcv_graphedge* edge = site->edges;
            while (edge) {
                center.x += edge->pos[0].x;
                center.y += edge->pos[0].y;
                count++;
                edge = edge->next;
            }
            if (count > 0) {
                points[site->index].x = center.x / (float)count;
                points[site->index].y = center.y / (float)count;
            }
        }
        jcv_diagram_free(&diagram);
    }

    m_Perlin = std::make_unique<siv::PerlinNoise>(static_cast<siv::PerlinNoise::seed_type>(config.seed));

    // Faction round-robin across all 16 playable types. NOTE: this is a
    // placeholder territory tag for the debug view only — terrain is neutral;
    // faction ownership is a gameplay property assigned elsewhere.
    static const BugClass kAllFactions[] = {
        BugClass::BeesWasps, BugClass::ButterfliesMoths, BugClass::Snails,
        BugClass::Mantis, BugClass::Spiders, BugClass::Fireflies, BugClass::Ants,
        BugClass::Termites, BugClass::CentipedesWorms, BugClass::MosquitosTicks,
        BugClass::Woodlice, BugClass::Dragonflies, BugClass::Bugs, BugClass::Roaches,
        BugClass::Beetles, BugClass::Scorpions
    };

    for (int i = 0; i < config.numTerrains; ++i) {
        TerrainData td;
        td.id = i;
        td.site = glm::vec2(points[i].x, points[i].y);
        td.bugClass = (i < 16) ? kAllFactions[i % 16] : BugClass::BossArena;

        switch (td.bugClass) {
            case BugClass::Ants:      td.color = glm::vec3(0.85f, 0.70f, 0.45f); break;
            case BugClass::Termites:  td.color = glm::vec3(0.70f, 0.50f, 0.35f); break;
            case BugClass::Spiders:   td.color = glm::vec3(0.35f, 0.30f, 0.25f); break;
            case BugClass::Woodlice:  td.color = glm::vec3(0.55f, 0.60f, 0.65f); break;
            case BugClass::BossArena: td.color = glm::vec3(0.55f, 0.05f, 0.05f); break;
            default:                  td.color = glm::vec3(0.5f);                 break;
        }

        td.spawnPoint = td.site;
        m_Terrains.push_back(td);
    }

    // -----------------------------------------------------------------------
    // Terraced tile grid (see docs/WORLDGEN_PLAN.md §2-3).
    // -----------------------------------------------------------------------
    GenerateTileGrid();
}

void WorldManager::GenerateTileGrid() {
    const auto& cfg = m_CurrentConfig;
    m_GridSize = std::max(1, (int)std::round((2.0f * cfg.worldExtent) / cfg.tileSize));
    m_Tiles.assign((size_t)m_GridSize * m_GridSize, TerrainTile{});

    if (m_Terrains.empty() || !m_Perlin) return;

    const int numTiers = std::max(1, cfg.numTiers);

    // --- Territory center positions in tile-grid space ----------------------
    const int N = (int)m_Terrains.size();
    std::vector<float> tcx(N), tcy(N);
    for (int i = 0; i < N; ++i) {
        tcx[i] = m_Terrains[i].site.x / (float)cfg.width  * (float)m_GridSize;
        tcy[i] = m_Terrains[i].site.y / (float)cfg.height * (float)m_GridSize;
    }

    // Each Voronoi cell covers gridSize²/N tiles → approximate cell radius.
    const float terrRadius = (float)m_GridSize / std::sqrt((float)N) * 0.55f;

    // === PASS 1: Territory assignment =========================================
    // Voronoi + domain warp to assign each tile to a biome, storing the two
    // nearest territory distances so the ridge boost can use them in pass 2.
    struct TileVD { int best=0, secondBest=0; float bestD=1e30f, secondD=1e30f; };
    std::vector<TileVD> vd((size_t)m_GridSize * m_GridSize);

    const float warpScale = 0.0025f;
    const float warpAmp   = cfg.width * 0.13f;

    for (int tz = 0; tz < m_GridSize; ++tz) {
        for (int tx = 0; tx < m_GridSize; ++tx) {
            TerrainTile& tile = m_Tiles[(size_t)tz * m_GridSize + tx];
            float px = ((float)tx / (float)m_GridSize) * (float)cfg.width;
            float py = ((float)tz / (float)m_GridSize) * (float)cfg.height;
            float wx = (float)(m_Perlin->octave2D_01(px*warpScale+0.0,  py*warpScale+0.0,  3)*2.0-1.0)*warpAmp;
            float wy = (float)(m_Perlin->octave2D_01(px*warpScale+31.4, py*warpScale+17.7, 3)*2.0-1.0)*warpAmp;
            float wpx = px+wx, wpy = py+wy;
            auto& v = vd[(size_t)tz * m_GridSize + tx];
            for (int i = 0; i < N; ++i) {
                float dx = wpx - m_Terrains[i].site.x, dy = wpy - m_Terrains[i].site.y;
                float d = dx*dx + dy*dy;
                if (d < v.bestD) { v.secondD=v.bestD; v.secondBest=v.best; v.bestD=d; v.best=i; }
                else if (d < v.secondD) { v.secondD=d; v.secondBest=i; }
            }
            tile.territoryId = (uint16_t)v.best;
        }
    }

    // === SADDLE CROSSING POINTS ===============================================
    // For each adjacent biome pair, find the border tile nearest to the midpoint
    // between the two territory centres.  The ridge boost is suppressed at these
    // tiles so the terrain naturally forms a traversable mountain pass instead of
    // an unbroken wall.  No post-gen ramp carving needed for cross-biome travel.
    struct BestSaddle { float distSq=1e30f; float sx=0, sz=0; };
    std::vector<BestSaddle> saddleMap((size_t)N * N);

    for (int tz = 0; tz < m_GridSize; ++tz) {
        for (int tx = 0; tx < m_GridSize; ++tx) {
            int ia = (int)m_Tiles[(size_t)tz * m_GridSize + tx].territoryId;
            // Check all 4 neighbours for a territory boundary.
            for (int d = 0; d < 4; ++d) {
                int nx=tx+kDX[d], nz=tz+kDZ[d];
                if (nx<0||nx>=m_GridSize||nz<0||nz>=m_GridSize) continue;
                int ib = (int)m_Tiles[(size_t)nz * m_GridSize + nx].territoryId;
                if (ia == ib) continue;
                int lo=std::min(ia,ib), hi=std::max(ia,ib);
                auto& bs = saddleMap[(size_t)lo * N + hi];
                // Score by distance from the midpoint between the two centres.
                float midX = (tcx[lo]+tcx[hi])*0.5f, midZ = (tcy[lo]+tcy[hi])*0.5f;
                float ddx=(float)tx-midX, ddz=(float)tz-midZ;
                float distSq = ddx*ddx + ddz*ddz;
                if (distSq < bs.distSq) { bs.distSq=distSq; bs.sx=(float)tx; bs.sz=(float)tz; }
            }
        }
    }

    // Flatten into a list for fast per-tile lookup in pass 2.
    std::vector<std::pair<float,float>> saddlePos;
    for (int lo=0; lo<N; ++lo)
        for (int hi=lo+1; hi<N; ++hi) {
            auto& bs = saddleMap[(size_t)lo*N+hi];
            if (bs.distSq < 1e29f) saddlePos.push_back({bs.sx, bs.sz});
        }

    // === PRE-BAKE: Saddle suppression per tile ================================
    // Separating this from the height loop keeps the hot path clean.
    // The suppression reaches 0 at each saddle and recovers to 1.0 at
    // kSaddleRadius tiles away — creating a smooth natural mountain pass.
    constexpr float kSaddleRadius = 12.f; // tiles — wider = gentler pass slopes
    std::vector<float> saddleSup((size_t)m_GridSize * m_GridSize, 1.0f);
    for (int tz = 0; tz < m_GridSize; ++tz) {
        for (int tx = 0; tx < m_GridSize; ++tx) {
            float mult = 1.0f;
            for (auto& [sx, sz] : saddlePos) {
                float ddx = (float)tx - sx, ddz = (float)tz - sz;
                float frac = std::sqrt(ddx*ddx + ddz*ddz) / kSaddleRadius;
                mult = std::min(mult, std::min(1.f, frac));
            }
            saddleSup[(size_t)tz * m_GridSize + tx] = mult;
        }
    }

    // === PASS 2: Height computation ===========================================
    for (int tz = 0; tz < m_GridSize; ++tz) {
        for (int tx = 0; tx < m_GridSize; ++tx) {
            TerrainTile& tile = m_Tiles[(size_t)tz * m_GridSize + tx];
            const auto& v = vd[(size_t)tz * m_GridSize + tx];
            int best=v.best, secondBest=v.secondBest;

            // --- Bowl tier ---------------------------------------------------
            float dToCenter = std::sqrt(
                (tx-tcx[best])*(tx-tcx[best]) + (tz-tcy[best])*(tz-tcy[best]));
            float nd = std::min(dToCenter / terrRadius, 1.0f);
            float sm = nd*nd*(3.0f-2.0f*nd);
            bool isBoss = (m_Terrains[best].bugClass == BugClass::BossArena);
            float bowlMid  = (float)(numTiers/2) - 1.f;
            float bowlTier = isBoss ? bowlMid+(1.f-sm)*1.f : bowlMid+sm*2.f;

            // --- Ridge boost -------------------------------------------------
            float sqrtBest   = std::sqrt(v.bestD);
            float sqrtSecond = (v.secondD < 1e29f) ? std::sqrt(v.secondD) : sqrtBest*4.f;
            float borderDist  = (sqrtSecond - sqrtBest) * 0.5f;
            float ridgeWidthPx = 8.f * ((float)cfg.width / (float)m_GridSize);
            float t = std::max(0.f, 1.f - borderDist / ridgeWidthPx);
            bool neighborIsBoss = (secondBest<N && m_Terrains[secondBest].bugClass==BugClass::BossArena);
            float ridgeBoost = (isBoss && neighborIsBoss) ? 0.f : std::pow(t, 0.25f) * 2.5f;
            ridgeBoost *= saddleSup[(size_t)tz * m_GridSize + tx];

            // --- Organic noise -----------------------------------------------
            double n = m_Perlin->octave2D_01((double)tx*cfg.tierNoiseScale,
                                              (double)tz*cfg.tierNoiseScale, 4);
            double edgeWarp = (m_Perlin->octave2D_01((double)tx*0.14+31.7,
                                                       (double)tz*0.14+31.7, 3)-0.5)*0.10;
            float noiseTier = std::clamp((float)((n+edgeWarp)*numTiers), 0.f, (float)(numTiers-1));

            float finalTier = bowlTier*0.35f + noiseTier*0.65f + ridgeBoost;
            tile.tier = (uint8_t)std::clamp((int)std::round(finalTier), 0, numTiers-1);
        }
    }

    // --- Water basins ---------------------------------------------------------
    // Tiers <= waterTier are flooded. Guarantee at least one pond: if the
    // lowest tier present is above waterTier, flood that tier instead.
    int minTier = numTiers - 1;
    for (const auto& t : m_Tiles) minTier = std::min(minTier, (int)t.tier);
    const int waterCutoff = std::max(cfg.waterTier, minTier);
    for (auto& t : m_Tiles)
        if ((int)t.tier <= waterCutoff) t.surface = TileSurface::Water;

    // --- Cliffs -----------------------------------------------------------------
    // A non-water tile that is strictly higher than a neighbour gets a cliff
    // face on that side.
    for (int tz = 0; tz < m_GridSize; ++tz) {
        for (int tx = 0; tx < m_GridSize; ++tx) {
            TerrainTile& tile = m_Tiles[(size_t)tz * m_GridSize + tx];
            if (tile.surface == TileSurface::Water) continue;
            for (int d = 0; d < 4; ++d) {
                int nx = tx + kDX[d], nz = tz + kDZ[d];
                if (nx < 0 || nx >= m_GridSize || nz < 0 || nz >= m_GridSize) continue;
                const TerrainTile& nb = m_Tiles[(size_t)nz * m_GridSize + nx];
                if ((int)tile.tier > (int)nb.tier) {
                    tile.surface = TileSurface::Cliff;
                    break;
                }
            }
        }
    }

    // --- Corridor Ramps -----------------------------------------------------------
    // Replaces the old single-tile random ramp system.
    //
    // Each crossing is a 2-tile-wide corridor of Ramp tiles that descends exactly
    // one tier per tile.  Multi-tier drops extend the corridor step by step as
    // long as the next pair of tiles in the descent direction are also cliff tiles
    // dropping by one tier.
    //
    // Algorithm:
    //   1. Collect candidate pairs: two adjacent cliff tiles (perpendicular to the
    //      slope) that both drop exactly one tier in the same direction and have at
    //      least one plateau tile of approach on each side.
    //   2. Score by total approach depth; select non-overlapping corridors with a
    //      minimum spacing so they spread naturally across the map.
    //   3. Carve: stamp each pair as Ramp and extend downward while conditions hold.
    //   4. Fallback: force a corridor for any tier-pair still not covered.

    // Count consecutive plateau tiles at `tier` walking in direction d from
    // (startX+kDX[d], startZ+kDZ[d]) outward (stops at wrong tier / cliff / water).
    auto countApproach = [&](int startX, int startZ, int d, int tier) -> int {
        int count = 0;
        for (int s = 1; s <= 6; ++s) {
            int cx = startX + kDX[d] * s, cz = startZ + kDZ[d] * s;
            if (cx < 0 || cx >= m_GridSize || cz < 0 || cz >= m_GridSize) break;
            const TerrainTile& t = m_Tiles[(size_t)cz * m_GridSize + cx];
            if ((int)t.tier != tier) break;
            if (t.surface == TileSurface::Water || t.surface == TileSurface::Cliff) break;
            ++count;
        }
        return count;
    };

    struct CorridorCandidate {
        int ax, az, bx, bz; // the two parallel cliff tiles (A and B)
        int pdx, pdz;        // unit perpendicular vector from A to B
        int slopeDir;        // direction of descent (rampDir)
        float score;
    };
    std::vector<CorridorCandidate> corridorCandidates;

    for (int tz = 0; tz < m_GridSize; ++tz) {
        for (int tx = 0; tx < m_GridSize; ++tx) {
            const TerrainTile& A = m_Tiles[(size_t)tz * m_GridSize + tx];
            if (A.surface != TileSurface::Cliff) continue;

            for (int sD = 0; sD < 4; ++sD) {
                // A's downhill neighbour must be exactly one tier lower, non-water.
                int dnx = tx + kDX[sD], dnz = tz + kDZ[sD];
                if (dnx < 0 || dnx >= m_GridSize || dnz < 0 || dnz >= m_GridSize) continue;
                const TerrainTile& Adown = m_Tiles[(size_t)dnz * m_GridSize + dnx];
                if (Adown.surface == TileSurface::Water) continue;
                if ((int)Adown.tier != (int)A.tier - 1) continue;

                // Perpendicular directions for this slope axis.
                int perpOpts[2];
                if (sD <= 1) { perpOpts[0] = 2; perpOpts[1] = 3; }
                else         { perpOpts[0] = 0; perpOpts[1] = 1; }

                for (int perpD : perpOpts) {
                    int bx = tx + kDX[perpD], bz = tz + kDZ[perpD];
                    if (bx < 0 || bx >= m_GridSize || bz < 0 || bz >= m_GridSize) continue;
                    // Deduplicate: only keep the candidate where B > A in grid order.
                    if (bx < tx || (bx == tx && bz < tz)) continue;

                    const TerrainTile& B = m_Tiles[(size_t)bz * m_GridSize + bx];
                    if (B.surface != TileSurface::Cliff || B.tier != A.tier) continue;

                    int pnx = bx + kDX[sD], pnz = bz + kDZ[sD];
                    if (pnx < 0 || pnx >= m_GridSize || pnz < 0 || pnz >= m_GridSize) continue;
                    const TerrainTile& Bdown = m_Tiles[(size_t)pnz * m_GridSize + pnx];
                    if (Bdown.surface == TileSurface::Water) continue;
                    if ((int)Bdown.tier != (int)B.tier - 1) continue;

                    // Approach quality: plateau depth on the uphill and downhill sides.
                    int upApp  = countApproach(tx, tz, sD ^ 1, (int)A.tier)
                               + countApproach(bx, bz, sD ^ 1, (int)B.tier);
                    int dnApp  = countApproach(dnx, dnz, sD, (int)Adown.tier)
                               + countApproach(pnx, pnz, sD, (int)Bdown.tier);
                    float score = (float)std::min(upApp, dnApp);
                    if (score < 1.f) continue; // must have at least one tile of approach on each side

                    corridorCandidates.push_back({tx, tz, bx, bz, kDX[perpD], kDZ[perpD], sD, score});
                }
            }
        }
    }

    // Sort best-scoring candidates first.
    std::sort(corridorCandidates.begin(), corridorCandidates.end(),
        [](const CorridorCandidate& a, const CorridorCandidate& b) { return a.score > b.score; });

    // Greedy selection: pick non-overlapping corridors with minimum spacing.
    constexpr int kCorridorMinSpacing = 10; // tiles
    std::vector<CorridorCandidate> selectedCorridors;
    for (auto& c : corridorCandidates) {
        bool tooClose = false;
        for (auto& s : selectedCorridors) {
            int dx = c.ax - s.ax, dz = c.az - s.az;
            if (dx * dx + dz * dz < kCorridorMinSpacing * kCorridorMinSpacing) {
                tooClose = true; break;
            }
        }
        if (!tooClose) selectedCorridors.push_back(c);
    }

    // Carve corridors into the tile grid.
    std::vector<bool> rampPairCovered((size_t)numTiers, false);

    auto carvePair = [&](int ax, int az, int bx, int bz, int sD) {
        auto& tA = m_Tiles[(size_t)az * m_GridSize + ax];
        auto& tB = m_Tiles[(size_t)bz * m_GridSize + bx];
        tA.surface = TileSurface::Ramp; tA.rampDir = (uint8_t)sD;
        tB.surface = TileSurface::Ramp; tB.rampDir = (uint8_t)sD;
        int lowerTier = (int)tA.tier - 1;
        if (lowerTier >= 0) rampPairCovered[(size_t)lowerTier] = true;
    };

    for (auto& c : selectedCorridors) {
        int ax = c.ax, az = c.az, bx = c.bx, bz = c.bz;
        const int sD = c.slopeDir;

        // Extend the corridor downward step by step as long as the next pair of
        // tiles are also cliff tiles dropping exactly one tier.
        for (int step = 0; step < numTiers; ++step) {
            auto& tA = m_Tiles[(size_t)az * m_GridSize + ax];
            auto& tB = m_Tiles[(size_t)bz * m_GridSize + bx];
            if (tA.surface != TileSurface::Cliff || tB.surface != TileSurface::Cliff) break;
            if (tA.tier != tB.tier) break;

            int nax = ax + kDX[sD], naz = az + kDZ[sD];
            int nbx = bx + kDX[sD], nbz = bz + kDZ[sD];
            if (nax < 0 || nax >= m_GridSize || naz < 0 || naz >= m_GridSize) break;
            if (nbx < 0 || nbx >= m_GridSize || nbz < 0 || nbz >= m_GridSize) break;
            const auto& downA = m_Tiles[(size_t)naz * m_GridSize + nax];
            const auto& downB = m_Tiles[(size_t)nbz * m_GridSize + nbx];
            if ((int)downA.tier != (int)tA.tier - 1) break;
            if ((int)downB.tier != (int)tB.tier - 1) break;
            if (downA.surface == TileSurface::Water || downB.surface == TileSurface::Water) break;

            carvePair(ax, az, bx, bz, sD);

            ax = nax; az = naz;
            bx = nbx; bz = nbz;
        }
    }

    // Fallback: force a corridor for any tier-pair still not covered.
    for (int lower = waterCutoff; lower < numTiers - 1; ++lower) {
        if (rampPairCovered[(size_t)lower]) continue;
        for (auto& c : corridorCandidates) {
            auto& tA = m_Tiles[(size_t)c.az * m_GridSize + c.ax];
            auto& tB = m_Tiles[(size_t)c.bz * m_GridSize + c.bx];
            if (tA.surface != TileSurface::Cliff || tB.surface != TileSurface::Cliff) continue;
            if ((int)tA.tier - 1 != lower) continue;
            carvePair(c.ax, c.az, c.bx, c.bz, c.slopeDir);
            break;
        }
    }

    // --- Buildable slots --------------------------------------------------------
    // Flat plateau tiles whose 4 neighbours are all in-bounds plateau tiles of
    // the same tier (interior, non-edge).
    for (int tz = 0; tz < m_GridSize; ++tz) {
        for (int tx = 0; tx < m_GridSize; ++tx) {
            TerrainTile& tile = m_Tiles[(size_t)tz * m_GridSize + tx];
            if (tile.surface != TileSurface::Plateau) continue;
            bool interior = true;
            for (int d = 0; d < 4 && interior; ++d) {
                int nx = tx + kDX[d], nz = tz + kDZ[d];
                if (nx < 0 || nx >= m_GridSize || nz < 0 || nz >= m_GridSize) { interior = false; break; }
                const TerrainTile& nb = m_Tiles[(size_t)nz * m_GridSize + nx];
                if (nb.surface != TileSurface::Plateau || nb.tier != tile.tier) interior = false;
            }
            tile.buildable = interior;
        }
    }

    // --- Spawn points ------------------------------------------------------------
    // Find the buildable tile nearest to the geometric centroid of each territory.
    // Using centroid instead of the Voronoi generator site keeps the spawn central
    // even after domain warping shifts the apparent territory shape.
    for (size_t i = 0; i < m_Terrains.size(); ++i) {
        if (m_Terrains[i].bugClass == BugClass::BossArena) continue;

        // Accumulate centroid over all tiles belonging to this territory.
        double sumTx = 0.0, sumTz = 0.0;
        int count = 0;
        for (int tz = 0; tz < m_GridSize; ++tz) {
            for (int tx = 0; tx < m_GridSize; ++tx) {
                if (m_Tiles[(size_t)tz * m_GridSize + tx].territoryId != (uint16_t)i) continue;
                sumTx += tx; sumTz += tz; ++count;
            }
        }
        if (count == 0) {
            spdlog::warn("WorldManager: territory {} has no tiles", i);
            continue;
        }
        float centX = (float)(sumTx / count);
        float centZ = (float)(sumTz / count);

        // Nearest buildable tile to that centroid.
        float bestD = 1e30f;
        glm::vec2 best = m_Terrains[i].spawnPoint;
        bool found = false;
        for (int tz = 0; tz < m_GridSize; ++tz) {
            for (int tx = 0; tx < m_GridSize; ++tx) {
                const TerrainTile& tile = m_Tiles[(size_t)tz * m_GridSize + tx];
                if (!tile.buildable || tile.territoryId != (uint16_t)i) continue;
                float dx = (float)tx - centX, dz = (float)tz - centZ;
                float d = dx * dx + dz * dz;
                if (d < bestD) { bestD = d; best = TileToWorld(tx, tz); found = true; }
            }
        }
        m_Terrains[i].spawnPoint = best;
        if (!found)
            spdlog::warn("WorldManager: territory {} has no buildable spawn tile", i);
    }

    static_assert(static_cast<int>(TileSurface::Water) == 3,
                  "Update counts[] array size if TileSurface enum changes");
    int counts[4] = {0, 0, 0, 0};
    for (const auto& t : m_Tiles) counts[(int)t.surface]++;
    spdlog::info("WorldManager: tile grid {}x{} - Plateau={} Cliff={} Ramp={} Water={} (waterCutoff tier={})",
                  m_GridSize, m_GridSize, counts[0], counts[1], counts[2], counts[3], waterCutoff);
}

void WorldManager::UpdateDebugTexture(SDL_GPUDevice* device, Texture** outTexture) {
    // Territory map preview: flat per-cell faction colour (nearest Voronoi
    // site). The heightmap-grayscale branch was removed with the organic
    // generator; this will move to flat tier colour bands once the
    // TerrainTile grid lands. See docs/WORLDGEN_PLAN.md.
    std::vector<unsigned char> pixels(m_CurrentConfig.width * m_CurrentConfig.height * 4);
    for (int y = 0; y < m_CurrentConfig.height; ++y) {
        for (int x = 0; x < m_CurrentConfig.width; ++x) {
            int pxIdx = (y * m_CurrentConfig.width + x) * 4;
            int s1_idx = -1;
            float d1 = 1e10f;
            for (int i = 0; i < (int)m_Terrains.size(); ++i) {
                float dx = (float)x - m_Terrains[i].site.x;
                float dy = (float)y - m_Terrains[i].site.y;
                float d2 = dx*dx + dy*dy;
                if (d2 < d1) { d1 = d2; s1_idx = i; }
            }
            glm::vec3 color = (s1_idx >= 0) ? m_Terrains[s1_idx].color : glm::vec3(0.f);
            pixels[pxIdx + 0] = (unsigned char)(color.r * 255);
            pixels[pxIdx + 1] = (unsigned char)(color.g * 255);
            pixels[pxIdx + 2] = (unsigned char)(color.b * 255);
            pixels[pxIdx + 3] = 255;
        }
    }
    if (*outTexture) delete *outTexture;
    *outTexture = new Texture(device, pixels.data(), m_CurrentConfig.width, m_CurrentConfig.height, TextureFilter::Nearest);
}
