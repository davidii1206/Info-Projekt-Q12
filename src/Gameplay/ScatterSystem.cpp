/**
 * @file ScatterSystem.cpp
 * @brief Implementation of biome-aware decorative prop scattering.
 */

#define GLM_ENABLE_EXPERIMENTAL
#include "ScatterSystem.h"
#include "Components.h"
#include "../Core/WorldManager.h"

#include <PerlinNoise.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>
#include <random>
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Default placeholder layer set
// ---------------------------------------------------------------------------

ScatterConfig ScatterConfig::Default(uint32_t seed) {
    ScatterConfig cfg;
    cfg.seed = seed;

    // Helper to keep the layer list readable.
    auto layer = [](const char* name, const char* modelPath, std::vector<BugClass> biomes,
                    float density, float minS, float maxS,
                    float clumpScale, float clumpThresh,
                    bool alignSlope) {
        ScatterLayer l;
        l.name = name;
        l.modelPath = modelPath;
        l.biomes = std::move(biomes);
        l.density = density;
        l.minScale = minS;
        l.maxScale = maxS;
        l.clumpNoiseScale = clumpScale;
        l.clumpThreshold = clumpThresh;
        l.alignToSlope = alignSlope;
        return l;
    };

    // --- Biome-agnostic ground litter -------------------------------------
    // Small grass tufts: dense, flat ground only, clumps into meadows.
    {
        auto l = layer("grass_tuft", "assets/prim_cone_green.glb", {}, 0.30f, 0.6f, 1.4f, 0.05f, 0.55f, false);
        l.maxSlope = 0.35f;
        cfg.layers.push_back(l);
    }
    // Scattered pebbles everywhere, including gentle slopes.
    {
        auto l = layer("pebble", "assets/prim_slab_grey.glb", {}, 0.10f, 0.3f, 0.8f, 0.08f, 0.0f, true);
        l.maxSlope = 0.8f;
        l.yOffset = -0.05f;
        cfg.layers.push_back(l);
    }
    // Larger rocks: rarer, prefer steeper ground, sit slightly sunk in.
    {
        auto l = layer("rock", "assets/prim_slab_grey.glb", {}, 0.05f, 1.0f, 2.5f, 0.03f, 0.55f, true);
        l.maxSlope = 1.0f;
        l.yOffset = -0.15f;
        cfg.layers.push_back(l);
    }

    // --- Ant biome: open trodden plazas, sparse weeds along the cracks ----
    {
        auto l = layer("weed_clover", "assets/prim_cone_green.glb", {BugClass::Ants}, 0.20f, 0.7f, 1.2f, 0.06f, 0.5f, false);
        l.maxSlope = 0.4f;
        cfg.layers.push_back(l);
    }

    // --- Termite biome: wood debris, bark chunks --------------------------
    {
        auto l = layer("wood_debris", "assets/prim_cylinder_brown.glb", {BugClass::Termites}, 0.30f, 0.8f, 1.6f, 0.05f, 0.3f, true);
        l.maxSlope = 0.6f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("bark_chunk", "assets/prim_cylinder_dark_brown.glb", {BugClass::Termites}, 0.18f, 0.6f, 1.3f, 0.07f, 0.4f, true);
        cfg.layers.push_back(l);
    }

    // --- Spider biome: dark detritus, web strands, mushrooms --------------
    {
        auto l = layer("dead_leaves", "assets/prim_slab_brown.glb", {BugClass::Spiders}, 0.40f, 0.7f, 1.5f, 0.05f, 0.35f, false);
        l.maxSlope = 0.5f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("mushroom", "assets/prim_sphere_red.glb", {BugClass::Spiders}, 0.12f, 0.6f, 1.8f, 0.09f, 0.6f, false);
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("web_strand", "assets/prim_cylinder_dark_brown.glb", {BugClass::Spiders}, 0.08f, 0.9f, 1.6f, 0.10f, 0.65f, false);
        cfg.layers.push_back(l);
    }

    // --- Woodlice biome: moss patches, twigs ------------------------------
    {
        auto l = layer("moss_patch", "assets/prim_sphere_green.glb", {BugClass::Woodlice}, 0.45f, 0.8f, 1.5f, 0.04f, 0.4f, false);
        l.maxSlope = 0.45f;
        l.yOffset = -0.05f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("twig", "assets/prim_cylinder_brown.glb", {BugClass::Woodlice}, 0.22f, 0.7f, 1.4f, 0.06f, 0.3f, true);
        cfg.layers.push_back(l);
    }

    return cfg;
}

// ---------------------------------------------------------------------------
// Sampling helpers
// ---------------------------------------------------------------------------

namespace {

/// BugClass of the territory owning a tile, or BugClass::None if out of range.
BugClass BiomeAt(const std::vector<TerrainData>& terrains, const TerrainTile& tile) {
    if (tile.territoryId >= terrains.size()) return BugClass::None;
    return terrains[tile.territoryId].bugClass;
}

} // namespace

// ---------------------------------------------------------------------------
// Populate
// ---------------------------------------------------------------------------

uint32_t ScatterSystem::Populate(entt::registry& registry, const WorldManager& world, const ScatterConfig& cfg) {
    const auto& wcfg = world.GetConfig();
    const auto& terrains = world.GetTerrains();

    if (world.GetGridSize() <= 0 || cfg.layers.empty()) {
        spdlog::warn("ScatterSystem: nothing to scatter (empty tile grid or no layers)");
        return 0;
    }

    const glm::vec2 worldSize = cfg.worldMax - cfg.worldMin;
    if (worldSize.x <= 0.f || worldSize.y <= 0.f || cfg.spacing <= 0.f) {
        spdlog::error("ScatterSystem: invalid world extents or spacing");
        return 0;
    }

    // Shared Perlin for clump masks (kept independent from the per-cell RNG so
    // clumping is a smooth field rather than white noise).
    siv::PerlinNoise perlin(static_cast<siv::PerlinNoise::seed_type>(cfg.seed));

    const int numTiers = std::max(1, wcfg.numTiers);

    const int cols = (int)std::ceil(worldSize.x / cfg.spacing);
    const int rows = (int)std::ceil(worldSize.y / cfg.spacing);

    uint32_t created = 0;

    for (int gz = 0; gz < rows && created < cfg.maxProps; ++gz) {
        for (int gx = 0; gx < cols && created < cfg.maxProps; ++gx) {
            // Deterministic per-cell RNG: depends only on seed + cell coords.
            std::mt19937 rng(cfg.seed ^ (uint32_t)(gx * 73856093) ^ (uint32_t)(gz * 19349663));
            std::uniform_real_distribution<float> u01(0.f, 1.f);

            // Jittered candidate position in world space.
            float jx = (u01(rng) - 0.5f) * cfg.jitter * cfg.spacing;
            float jz = (u01(rng) - 0.5f) * cfg.jitter * cfg.spacing;
            float wx = cfg.worldMin.x + (gx + 0.5f) * cfg.spacing + jx;
            float wz = cfg.worldMin.y + (gz + 0.5f) * cfg.spacing + jz;
            if (wx > cfg.worldMax.x || wz > cfg.worldMax.y) continue;

            // Tile lookup: skip cliffs and water entirely (no scatter there).
            int tx, tz;
            world.WorldToTile(wx, wz, tx, tz);
            const TerrainTile& tile = world.GetTile(tx, tz);
            if (tile.surface == TileSurface::Cliff || tile.surface == TileSurface::Water) continue;

            // Height band is normalized [0..1] across the tier range; slope is
            // binary - 0 on flat Plateau tiles, 1 on Ramp tiles (see
            // docs/WORLDGEN_PLAN.md §6).
            float hC = (numTiers > 1) ? (float)tile.tier / (float)(numTiers - 1) : 0.f;
            float slope = (tile.surface == TileSurface::Ramp) ? 1.0f : 0.0f;
            glm::vec3 normal(0.f, 1.f, 0.f);

            BugClass biome = BiomeAt(terrains, tile);

            // Evaluate layers in order; first match wins this candidate.
            for (size_t li = 0; li < cfg.layers.size(); ++li) {
                const ScatterLayer& L = cfg.layers[li];

                if (!L.biomes.empty() &&
                    std::find(L.biomes.begin(), L.biomes.end(), biome) == L.biomes.end())
                    continue;
                if (hC < L.minHeight || hC > L.maxHeight) continue;
                if (slope > L.maxSlope) continue;

                if (L.clumpThreshold > 0.f) {
                    float c = (float)perlin.octave2D_01(wx * L.clumpNoiseScale,
                                                        wz * L.clumpNoiseScale, 3);
                    if (c < L.clumpThreshold) continue;
                }

                if (u01(rng) > L.density) continue;

                // --- Place the prop ---------------------------------------
                float worldY = world.TierToWorldHeight(tile.tier) * cfg.heightWorldScale + L.yOffset;
                glm::vec3 pos(wx, worldY, wz);

                glm::vec3 rot(0.f);
                if (L.randomYaw) rot.y = u01(rng) * 360.f;
                if (L.alignToSlope) {
                    // Approximate tilt from the surface normal (degrees).
                    rot.x = glm::degrees(std::atan2(normal.z, normal.y));
                    rot.z = glm::degrees(-std::atan2(normal.x, normal.y));
                }

                float s = glm::mix(L.minScale, L.maxScale, u01(rng));

                auto e = registry.create();
                auto& tf = registry.emplace<TransformComponent>(e, pos);
                tf.rotation = rot;
                tf.scale = glm::vec3(s);
                registry.emplace<ModelComponent>(e, L.modelPath);
                registry.emplace<ScatterPropComponent>(e, (uint16_t)li);
                ++created;
                break; // candidate consumed
            }
        }
    }

    spdlog::info("ScatterSystem: placed {} props across {} layers ({}x{} candidate grid)",
                 created, cfg.layers.size(), cols, rows);
    return created;
}

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

void ScatterSystem::Clear(entt::registry& registry) {
    auto view = registry.view<ScatterPropComponent>();
    std::vector<entt::entity> doomed(view.begin(), view.end());
    registry.destroy(doomed.begin(), doomed.end());
    spdlog::info("ScatterSystem: cleared {} scatter props", doomed.size());
}
