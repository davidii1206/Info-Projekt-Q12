/**
 * @file TerrainMeshBuilder.h
 * @brief Builds a CPU-side mesh (vertices + indices) from a WorldManager
 *        terraced tile grid (see docs/WORLDGEN_PLAN.md §2-4, TODO A).
 *
 * The output is plain ModelVertex/uint32_t data, suitable both for
 * AssetManager::RegisterProceduralScene() (rendering) and
 * MeshCollisionBuilder::Build() (physics collision) - no GPU work happens
 * here.
 */
#pragma once
#include <vector>
#include "Model.h"

class WorldManager;

namespace TerrainMeshBuilder {

/// CPU-side terrain mesh: plateau tops, cliff walls and ramp wedges, flat
/// shaded with per-tier vertex colors (see ModelVertex::color).
struct TerrainMeshData {
    std::vector<ModelVertex> vertices;
    std::vector<uint32_t> indices;
};

/// Builds the full-grid terrain mesh from world.GetTiles(). Deterministic -
/// every peer that generated the same WorldManager produces the same mesh.
TerrainMeshData Build(const WorldManager& world);

} // namespace TerrainMeshBuilder
