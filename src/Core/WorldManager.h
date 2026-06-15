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

struct WorldGenConfig {
    int numTerrains = 8;
    int seed = 12345;
    int relaxationIterations = 2;
    int width = 1024;
    int height = 1024;

    // --- Terraced model (see docs/WORLDGEN_PLAN.md) ---
    // Organic-height fields (noiseScale, slopeSharpness, slopeWidth,
    // altitudeNoiseScale, altitudeMaxHeight, numAltitudeLevels, ...) were
    // removed during the worldgen teardown. The terraced tier config
    // (numTiers / tierHeight) is added in the implementation phase.
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

    // Helper for visualization
    void UpdateDebugTexture(SDL_GPUDevice* device, class Texture** outTexture);

private:
    void Clear();

    WorldGenConfig m_CurrentConfig;
    std::vector<TerrainData> m_Terrains;
    std::vector<float> m_Heightmap; // temporary flat placeholder (see above)
    std::unique_ptr<siv::PerlinNoise> m_Perlin;
};
