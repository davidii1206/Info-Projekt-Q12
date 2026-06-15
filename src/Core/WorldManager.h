#pragma once
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <SDL3/SDL_gpu.h>
#include <PerlinNoise.hpp>
#include <jc_voronoi.h>
#include "../Gameplay/Bug_classes.h"

struct TerrainData {
    int id;
    BugClass bugClass;
    glm::vec2 site;
    glm::vec2 spawnPoint;
    glm::vec3 color;
};

/// Surface kind of a single terrain tile (see docs/WORLDGEN_PLAN.md §2).
enum class TileSurface : uint8_t { Plateau, Cliff, Ramp, Water };

/// One cell of the terraced terrain grid. World height for a Plateau/Cliff
/// tile is `tier * tierHeight`; Ramp tiles interpolate to the lower
/// neighbour indicated by `rampDir`.
struct TerrainTile {
    uint8_t     tier        = 0;                 ///< Terrace level 0..numTiers-1.
    TileSurface surface     = TileSurface::Plateau;
    uint16_t    territoryId = 0xFFFF;            ///< Index into m_Terrains, or 0xFFFF if unowned (water/ramp).
    uint8_t     rampDir     = 0;                 ///< Descent direction for Ramp tiles: 0=+X, 1=-X, 2=+Z, 3=-Z.
    bool        buildable   = false;             ///< Flat, dry, interior plateau tile.
};

struct WorldGenConfig {
    int numTerrains = 8;
    int seed = 12345;
    int relaxationIterations = 2;
    int width = 1024;
    int height = 1024;

    // --- Terraced model (see docs/WORLDGEN_PLAN.md) ---
    // Organic-height fields (noiseScale, slopeSharpness, slopeWidth,
    // altitudeNoiseScale, altitudeMaxHeight, numAltitudeLevels, ...) were
    // removed during the worldgen teardown.

    /// World spans [-worldExtent, +worldExtent] on X and Z (matches
    /// FogOfWar / TerritorySystem / ScatterSystem extents).
    float worldExtent = 150.f;
    /// World units per tile edge. 2.0 matches FogOfWar's cell size, so tiles
    /// and fog cells line up 1:1.
    float tileSize = 2.0f;
    /// Number of discrete terrace levels (0..numTiers-1).
    int numTiers = 4;
    /// World units of height per terrace level.
    float tierHeight = 2.0f;
    /// Tiers <= waterTier are flooded (TileSurface::Water).
    int waterTier = 0;
    /// Frequency of the low-frequency Perlin noise used for tier assignment
    /// (sampled in tile-grid space).
    float tierNoiseScale = 0.045f;
    /// Approximate spacing (in tiles) between ramps along a cliff edge.
    int rampSpacing = 6;
};

class WorldManager {
public:
    WorldManager();
    ~WorldManager();

    void Generate(const WorldGenConfig& config);

    const std::vector<TerrainData>& GetTerrains() const { return m_Terrains; }
    const WorldGenConfig& GetConfig() const { return m_CurrentConfig; }

    // TEMPORARY: flat (all-zero) heightmap kept so ScatterSystem keeps
    // building during the worldgen teardown. Replaced by a TerrainTile grid
    // in the implementation phase (see docs/WORLDGEN_PLAN.md §2, §7).
    const std::vector<float>& GetHeightmap() const { return m_Heightmap; }

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

    // Helper for visualization
    void UpdateDebugTexture(SDL_GPUDevice* device, class Texture** outTexture);

private:
    void Clear();
    void GenerateTileGrid();

    WorldGenConfig m_CurrentConfig;
    std::vector<TerrainData> m_Terrains;
    std::vector<float> m_Heightmap; // temporary flat placeholder (see above)
    std::unique_ptr<siv::PerlinNoise> m_Perlin;

    int m_GridSize = 0;
    std::vector<TerrainTile> m_Tiles;
};
