/**
 * @file WorldManager.h
 * @brief Procedural world generation: Voronoi territories + a terraced tile grid.
 */

#pragma once
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <SDL3/SDL_gpu.h>
#include <PerlinNoise.hpp>
#include "../Gameplay/Bug_classes.h"

/**
 * @struct TerrainData
 * @brief One Voronoi territory: its site, biome faction, spawn point and colour.
 */
struct TerrainData {
    int id;             ///< Territory index.
    BugClass bugClass;  ///< Biome faction assigned to this territory.
    glm::vec2 site;     ///< Voronoi generator site (in width×height map space).
    glm::vec2 spawnPoint; ///< Buildable spawn position (world XZ).
    glm::vec3 color;    ///< Debug/overlay colour.
};

/// Surface kind of a single terrain tile (see docs/WORLDGEN_PLAN.md §2).
enum class TileSurface : uint8_t { Plateau, Cliff, Ramp, Water };

/// One cell of the terraced terrain grid. World height for a Plateau/Cliff
/// tile is `tier * tierHeight`; Ramp tiles interpolate to the lower
/// neighbour indicated by `rampDir`.
struct TerrainTile {
    uint8_t     tier        = 0;                 ///< Terrace level 0..numTiers-1.
    TileSurface surface     = TileSurface::Plateau; ///< Surface kind of this tile.
    uint16_t    territoryId = 0xFFFF;            ///< Index into m_Terrains, or 0xFFFF if unowned (water/ramp).
    uint8_t     rampDir     = 0;                 ///< Descent direction for Ramp tiles: 0=+X, 1=-X, 2=+Z, 3=-Z.
    bool        buildable   = false;             ///< Flat, dry, interior plateau tile.
};

/**
 * @struct WorldGenConfig
 * @brief Tunable parameters for world generation.
 */
struct WorldGenConfig {
    int numTerrains = 25;          ///< Number of Voronoi territories.
    int seed = 12345;              ///< Deterministic generation seed.
    int relaxationIterations = 2;  ///< Lloyd relaxation passes for even territories.
    int width = 1024;              ///< Voronoi/debug map width.
    int height = 1024;             ///< Voronoi/debug map height.

    // --- Terraced model (see docs/WORLDGEN_PLAN.md) ---
    // Organic-height fields (noiseScale, slopeSharpness, slopeWidth,
    // altitudeNoiseScale, altitudeMaxHeight, numAltitudeLevels, ...) were
    // removed during the worldgen teardown.

    /// World spans [-worldExtent, +worldExtent] on X and Z (matches
    /// FogOfWar / TerritorySystem / ScatterSystem extents).
    float worldExtent = 250.f;
    /// World units per tile edge.
    float tileSize = 1.0f;
    /// Number of discrete terrace levels (0..numTiers-1).
    int numTiers = 6;
    /// World units of height per terrace level.
    float tierHeight = 2.0f;
    /// Tiers <= waterTier are flooded (TileSurface::Water).
    int waterTier = 0;
    /// Frequency of the low-frequency Perlin noise used for tier assignment
    /// (sampled in tile-grid space — halved vs 2.0 tileSize to preserve world-space feature size).
    float tierNoiseScale = 0.0325f;
};

/**
 * @class WorldManager
 * @brief Generates and stores the procedural world (territories + tile grid)
 *        and provides world↔tile coordinate helpers.
 */
class WorldManager {
public:
    WorldManager();  ///< Constructs an empty world.
    ~WorldManager(); ///< Destructor.

    /// @brief Generates the world (Voronoi territories + terraced tile grid).
    /// @param config Generation parameters.
    void Generate(const WorldGenConfig& config);

    /// @return The generated territories.
    const std::vector<TerrainData>& GetTerrains() const { return m_Terrains; }
    /// @return The configuration used for the current world.
    const WorldGenConfig& GetConfig() const { return m_CurrentConfig; }

    // --- Terraced tile grid (see docs/WORLDGEN_PLAN.md §2-3) ---

    /// Number of tiles along one edge of the (square) tile grid.
    int GetGridSize() const { return m_GridSize; }

    /// Flat row-major tile array, size GetGridSize()^2 (index = z * gridSize + x).
    const std::vector<TerrainTile>& GetTiles() const { return m_Tiles; }

    /// Tile at grid coordinates (tx, tz); out-of-range coordinates are clamped.
    const TerrainTile& GetTile(int tx, int tz) const;

    /// Converts world-space XZ to tile grid coordinates (clamped to grid bounds).
    void WorldToTile(float wx, float wz, int& outTx, int& outTz) const;

    /// Converts tile grid coordinates to the world-space XZ centre of the tile.
    glm::vec2 TileToWorld(int tx, int tz) const;

    /// World-space Y for a given terrace tier.
    float TierToWorldHeight(int tier) const { return (float)tier * m_CurrentConfig.tierHeight; }

    /// @brief Renders a flat territory-colour preview into a GPU texture.
    /// @param device     GPU device.
    /// @param outTexture In/out: replaced with the freshly rendered preview texture.
    void UpdateDebugTexture(SDL_GPUDevice* device, class Texture** outTexture);

private:
    void Clear();            ///< Frees territories and tiles.
    void GenerateTileGrid(); ///< Builds the terraced tile grid from the territories.

    WorldGenConfig m_CurrentConfig;            ///< Config of the current world.
    std::vector<TerrainData> m_Terrains;       ///< Generated territories.
    std::unique_ptr<siv::PerlinNoise> m_Perlin; ///< Noise source for warp/heights.

    int m_GridSize = 0;                        ///< Tiles per grid edge.
    std::vector<TerrainTile> m_Tiles;          ///< Row-major tile grid.
};
