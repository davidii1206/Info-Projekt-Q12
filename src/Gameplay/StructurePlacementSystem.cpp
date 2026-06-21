#include "StructurePlacementSystem.h"
#include "Components.h"
#include "../Core/WorldManager.h"
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>
#include <random>
#include <vector>
#include <cmath>

namespace {

bool TooClose(const std::vector<glm::vec2>& occupied, glm::vec2 pos, float minDist) {
    for (const auto& o : occupied) {
        float dx = o.x - pos.x, dy = o.y - pos.y;
        if (std::sqrt(dx * dx + dy * dy) < minDist) return true;
    }
    return false;
}

} // namespace

namespace StructurePlacementSystem {

uint32_t Place(entt::registry& registry, const WorldManager& world,
               const StructurePlacementConfig& cfg) {
    const auto& terrains = world.GetTerrains();
    uint32_t placed = 0;
    std::vector<glm::vec2> occupied;

    // ------------------------------------------------------------------
    // Faction bases: one per territory at its pre-computed spawn tile
    // (WorldManager::GenerateTileGrid already found the nearest buildable
    // tile to each Voronoi site and stored its world-XZ in spawnPoint).
    // ------------------------------------------------------------------
    for (const auto& td : terrains) {
        glm::vec2 sp = td.spawnPoint;
        int tx, tz;
        world.WorldToTile(sp.x, sp.y, tx, tz);
        const TerrainTile& tile = world.GetTile(tx, tz);
        if (!tile.buildable) continue;

        float worldY = world.TierToWorldHeight(tile.tier);
        auto e = registry.create();
        auto& tf = registry.emplace<TransformComponent>(e, glm::vec3(sp.x, worldY, sp.y));
        tf.scale = glm::vec3(cfg.baseModelScale);
        registry.emplace<ModelComponent>(e, std::string(cfg.baseModelPath));
        registry.emplace<StructureComponent>(e, (uint16_t)td.id, /*isFactionBase=*/true);
        occupied.push_back(sp);
        ++placed;
    }

    // ------------------------------------------------------------------
    // Neutral resource nodes: walk all buildable tiles, sample sparsely,
    // enforce min spacing from everything already placed.
    // ------------------------------------------------------------------
    const int gs = world.GetGridSize();
    const int maxTotal = (int)terrains.size() + cfg.maxNeutralNodes;
    std::mt19937 rng(cfg.seed ^ 0xDEADBEEFu);
    std::uniform_real_distribution<float> u01(0.f, 1.f);
    constexpr float kSampleRate = 0.04f; // evaluate ~4% of buildable tiles

    for (int tz = 0; tz < gs && (int)placed < maxTotal; ++tz) {
        for (int tx = 0; tx < gs && (int)placed < maxTotal; ++tx) {
            const TerrainTile& tile = world.GetTile(tx, tz);
            if (!tile.buildable) continue;
            if (u01(rng) > kSampleRate) continue;

            glm::vec2 wp = world.TileToWorld(tx, tz);
            if (TooClose(occupied, wp, cfg.nodeMinSpacing)) continue;

            float worldY = world.TierToWorldHeight(tile.tier);
            auto e = registry.create();
            auto& tf = registry.emplace<TransformComponent>(e, glm::vec3(wp.x, worldY, wp.y));
            tf.scale = glm::vec3(cfg.nodeModelScale);
            registry.emplace<ModelComponent>(e, std::string(cfg.nodeModelPath));
            registry.emplace<StructureComponent>(e, tile.territoryId, /*isFactionBase=*/false);
            occupied.push_back(wp);
            ++placed;
        }
    }

    const uint32_t bases = std::min((uint32_t)terrains.size(), placed);
    spdlog::info("StructurePlacementSystem: placed {} structures ({} faction bases, {} neutral nodes)",
                 placed, bases, placed - bases);
    return placed;
}

void Clear(entt::registry& registry) {
    auto view = registry.view<StructureComponent>();
    std::vector<entt::entity> doomed(view.begin(), view.end());
    registry.destroy(doomed.begin(), doomed.end());
    spdlog::info("StructurePlacementSystem: cleared {} structures", doomed.size());
}

} // namespace StructurePlacementSystem
