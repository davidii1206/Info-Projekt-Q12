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
    /// Small deterministic hash used for per-tile ramp placement decisions.
    uint32_t TileHash(uint32_t seed, int tx, int tz) {
        uint32_t h = seed;
        h ^= (uint32_t)tx * 0x9E3779B1u;
        h ^= (uint32_t)tz * 0x85EBCA77u;
        h ^= h >> 15;
        h *= 0x27D4EB2Fu;
        h ^= h >> 13;
        return h;
    }

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
        td.bugClass = kAllFactions[i % 16];

        switch (td.bugClass) {
            case BugClass::Ants:     td.color = glm::vec3(0.85f, 0.70f, 0.45f); break;
            case BugClass::Termites: td.color = glm::vec3(0.70f, 0.50f, 0.35f); break;
            case BugClass::Spiders:  td.color = glm::vec3(0.35f, 0.30f, 0.25f); break;
            case BugClass::Woodlice: td.color = glm::vec3(0.55f, 0.60f, 0.65f); break;
            default:                 td.color = glm::vec3(0.5f);                break;
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

    // --- Per-territory base tier (low-frequency Perlin at the site) --------
    // Bounds each tile's tier to within +-1 of its territory's base tier so
    // neighbouring territories tend to share a tier (no floating islands).
    std::vector<int> territoryBaseTier(m_Terrains.size());
    for (size_t i = 0; i < m_Terrains.size(); ++i) {
        float sx = m_Terrains[i].site.x / (float)cfg.width  * (float)m_GridSize;
        float sy = m_Terrains[i].site.y / (float)cfg.height * (float)m_GridSize;
        double n = m_Perlin->octave2D_01(sx * cfg.tierNoiseScale, sy * cfg.tierNoiseScale, 4);
        territoryBaseTier[i] = std::clamp((int)(n * numTiers), 0, numTiers - 1);
    }

    // --- Tier + territory assignment per tile -------------------------------
    for (int tz = 0; tz < m_GridSize; ++tz) {
        for (int tx = 0; tx < m_GridSize; ++tx) {
            TerrainTile& tile = m_Tiles[(size_t)tz * m_GridSize + tx];

            // Nearest Voronoi site (pixel space) determines the owning territory.
            float px = ((float)tx / (float)m_GridSize) * (float)cfg.width;
            float py = ((float)tz / (float)m_GridSize) * (float)cfg.height;
            int   best = 0;
            float bestD = 1e30f;
            for (size_t i = 0; i < m_Terrains.size(); ++i) {
                float dx = px - m_Terrains[i].site.x;
                float dy = py - m_Terrains[i].site.y;
                float d = dx * dx + dy * dy;
                if (d < bestD) { bestD = d; best = (int)i; }
            }
            tile.territoryId = (uint16_t)best;

            double n = m_Perlin->octave2D_01(tx * cfg.tierNoiseScale, tz * cfg.tierNoiseScale, 4);
            int rawTier = std::clamp((int)(n * numTiers), 0, numTiers - 1);

            int baseTier = territoryBaseTier[best];
            int tier = std::clamp(rawTier, baseTier - 1, baseTier + 1);
            tile.tier = (uint8_t)std::clamp(tier, 0, numTiers - 1);
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

    // --- Ramps --------------------------------------------------------------------
    // Convert a roughly-every-`rampSpacing`-tiles cliff tile that descends by
    // exactly one tier into a Ramp, deterministically per seed+position.
    // Afterwards guarantee at least one ramp exists between every pair of
    // vertically-adjacent tiers so every tier is reachable on foot.
    std::vector<bool> rampPairCovered((size_t)numTiers, false); // index = lower tier of the pair
    auto tryRamp = [&](int tx, int tz, bool force) -> bool {
        TerrainTile& tile = m_Tiles[(size_t)tz * m_GridSize + tx];
        if (tile.surface != TileSurface::Cliff) return false;
        for (int d = 0; d < 4; ++d) {
            int nx = tx + kDX[d], nz = tz + kDZ[d];
            if (nx < 0 || nx >= m_GridSize || nz < 0 || nz >= m_GridSize) continue;
            const TerrainTile& nb = m_Tiles[(size_t)nz * m_GridSize + nx];
            if (nb.surface == TileSurface::Water) continue;
            if ((int)nb.tier == (int)tile.tier - 1) {
                bool select = force || (TileHash((uint32_t)cfg.seed, tx, tz) % (uint32_t)cfg.rampSpacing == 0);
                if (select) {
                    tile.surface = TileSurface::Ramp;
                    tile.rampDir = (uint8_t)d;
                    rampPairCovered[nb.tier] = true;
                    return true;
                }
            }
        }
        return false;
    };

    for (int tz = 0; tz < m_GridSize; ++tz)
        for (int tx = 0; tx < m_GridSize; ++tx)
            tryRamp(tx, tz, false);

    for (int lower = waterCutoff; lower < numTiers - 1; ++lower) {
        if (rampPairCovered[lower]) continue;
        for (int tz = 0; tz < m_GridSize && !rampPairCovered[lower]; ++tz)
            for (int tx = 0; tx < m_GridSize && !rampPairCovered[lower]; ++tx) {
                TerrainTile& tile = m_Tiles[(size_t)tz * m_GridSize + tx];
                if (tile.surface != TileSurface::Cliff || (int)tile.tier - 1 != lower) continue;
                tryRamp(tx, tz, true);
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
    // Nearest buildable tile of each territory to its Voronoi site becomes the
    // faction's home plateau (overwrites TerrainData::spawnPoint with world coords).
    for (size_t i = 0; i < m_Terrains.size(); ++i) {
        float sx = m_Terrains[i].site.x / (float)cfg.width  * (float)m_GridSize;
        float sy = m_Terrains[i].site.y / (float)cfg.height * (float)m_GridSize;
        float bestD = 1e30f;
        glm::vec2 best = TileToWorld((int)sx, (int)sy);
        bool found = false;
        for (int tz = 0; tz < m_GridSize; ++tz) {
            for (int tx = 0; tx < m_GridSize; ++tx) {
                const TerrainTile& tile = m_Tiles[(size_t)tz * m_GridSize + tx];
                if (!tile.buildable || tile.territoryId != (uint16_t)i) continue;
                float dx = (float)tx - sx, dy = (float)tz - sy;
                float d = dx * dx + dy * dy;
                if (d < bestD) { bestD = d; best = TileToWorld(tx, tz); found = true; }
            }
        }
        m_Terrains[i].spawnPoint = best;
        if (!found)
            spdlog::warn("WorldManager: territory {} has no buildable spawn tile", i);
    }

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
