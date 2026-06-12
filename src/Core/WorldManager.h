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
    int altitude = 0;
    glm::vec2 site;
    glm::vec2 spawnPoint;
    glm::vec3 color;
    std::vector<glm::vec2> vertices;
    std::vector<glm::vec2> subSites; // For Ant biome polygons
};

struct WorldGenConfig {
    int numTerrains = 8;
    int seed = 12345;
    float noiseScale = 0.05f;
    int noiseOctaves = 4;
    int relaxationIterations = 2;
    bool randomSpawnInTerrain = false;
    int width = 1024;
    int height = 1024;

    // Thronefall Style
    int numAltitudeLevels = 4;
    float altitudeNoiseScale = 0.01f;
    float altitudeMaxHeight = 1.0f;
    float slopeSharpness = 0.1f; // Transition sharpness
    float slopeWidth = 25.0f;    // Base width of ramps
    bool showHeightmap = true;
};

class WorldManager {
public:
    WorldManager();
    ~WorldManager();

    void Generate(const WorldGenConfig& config);
    
    const std::vector<TerrainData>& GetTerrains() const { return m_Terrains; }
    const WorldGenConfig& GetConfig() const { return m_CurrentConfig; }
    const std::vector<float>& GetHeightmap() const { return m_Heightmap; }

    // Helper for visualization
    void UpdateDebugTexture(SDL_GPUDevice* device, class Texture** outTexture);

private:
    void Clear();
    void GenerateHeightmap();
    
    // Biome-specific height generators
    float GetAntHeight(float x, float y, const jcv_diagram* diagram);
    float GetTermiteHeight(float x, float y);
    float GetSpiderHeight(float x, float y);
    float GetWoodliceHeight(float x, float y);

    WorldGenConfig m_CurrentConfig;
    std::vector<TerrainData> m_Terrains;
    std::vector<float> m_Heightmap;
    std::unique_ptr<siv::PerlinNoise> m_Perlin;
};
