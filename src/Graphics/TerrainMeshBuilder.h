/**
 * @file TerrainMeshBuilder.h
 * @brief Builds CPU-side meshes (vertices + indices) from a WorldManager
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
    std::vector<ModelVertex> vertices; ///< CPU-side terrain vertices.
    std::vector<uint32_t> indices;     ///< Triangle indices into @ref vertices.
};

/// Builds the full-grid terrain mesh from world.GetTiles(). Deterministic -
/// every peer that generated the same WorldManager produces the same mesh.
/// Use this for physics collision (single merged MeshShape).
TerrainMeshData Build(const WorldManager& world);

/// Default tile-edge length for per-chunk terrain meshes.
inline constexpr int kDefaultChunkSize = 32;

/// Result of BuildChunks: one TerrainMeshData per chunk plus metadata.
struct ChunkBuildResult {
    std::vector<TerrainMeshData> chunks;   ///< Per-chunk mesh data, row-major.
    int chunksPerAxis = 0;                   ///< Chunks along one grid edge.
    int chunkSize = kDefaultChunkSize;       ///< Tiles per chunk edge.
};

/// Splits the terrain into chunkSize×chunkSize tile meshes. Each chunk
/// builds surface + cliff walls for its tiles. Corner colours are computed
/// globally so seams are watertight across chunk boundaries.
/// Use these for rendering (per-chunk fog/frustum culling).
ChunkBuildResult BuildChunks(const WorldManager& world, int chunkSize = kDefaultChunkSize);

} // namespace TerrainMeshBuilder
