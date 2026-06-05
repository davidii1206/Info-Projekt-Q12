#include "WorldManager.h"
#include "../Graphics/API/Texture.h"
#include <random>
#include <cstring>
#include <algorithm>
#include <cmath>

#define JC_VORONOI_IMPLEMENTATION
#include <jc_voronoi.h>

WorldManager::WorldManager() {}
WorldManager::~WorldManager() { Clear(); }

void WorldManager::Clear() {
    m_Terrains.clear();
    m_Heightmap.clear();
}

void WorldManager::Generate(const WorldGenConfig& config) {
    m_CurrentConfig = config;
    Clear();

    m_Perlin = std::make_unique<siv::PerlinNoise>(static_cast<siv::PerlinNoise::seed_type>(config.seed));
    std::mt19937 rng(config.seed);

    std::vector<jcv_point> points(16);
    for (int q = 0; q < 4; ++q) {
        float qX = (q % 2 == 0) ? 0.0f : config.width * 0.5f;
        float qY = (q < 2) ? 0.0f : config.height * 0.5f;
        for (int i = 0; i < 4; ++i) {
            float offsetX = (i % 2 == 0) ? config.width * 0.125f : config.width * 0.375f;
            float offsetY = (i < 2) ? config.height * 0.125f : config.height * 0.375f;
            std::uniform_real_distribution<float> jitter(-30.0f, 30.0f);
            points[q * 4 + i].x = qX + offsetX + jitter(rng);
            points[q * 4 + i].y = qY + offsetY + jitter(rng);
        }
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

    std::vector<TerrainData> rawTerrains;
    for (int i = 0; i < 16; ++i) {
        TerrainData td;
        td.site = glm::vec2(points[i].x, points[i].y);
        rawTerrains.push_back(td);
    }

    std::sort(rawTerrains.begin(), rawTerrains.end(), [](const TerrainData& a, const TerrainData& b) {
        if (std::abs(a.site.y - b.site.y) > 10.0f) return a.site.y < b.site.y;
        return a.site.x < b.site.x;
    });

    auto assign = [&](int idx, BiomeType b, BugClass c) {
        rawTerrains[idx].biomeType = b;
        rawTerrains[idx].bugClass = c;
    };

    assign(0, BiomeType::Wetland, BugClass::Dragonflies);
    assign(1, BiomeType::Wetland, BugClass::Snails);
    assign(2, BiomeType::Desert, BugClass::Beetles);
    assign(3, BiomeType::Desert, BugClass::Bugs);
    assign(4, BiomeType::Wetland, BugClass::MosquitosTicks);
    assign(5, BiomeType::Wetland, BugClass::Woodlice);
    assign(6, BiomeType::Desert, BugClass::Scorpions);
    assign(7, BiomeType::Desert, BugClass::Roaches);
    assign(8, BiomeType::MushroomForest, BugClass::Termites);
    assign(9, BiomeType::MushroomForest, BugClass::Spiders);
    assign(10, BiomeType::HiveGlade, BugClass::BeesWasps);
    assign(11, BiomeType::HiveGlade, BugClass::ButterfliesMoths);
    assign(12, BiomeType::MushroomForest, BugClass::Ants);
    assign(13, BiomeType::MushroomForest, BugClass::CentipedesWorms);
    assign(14, BiomeType::HiveGlade, BugClass::Fireflies);
    assign(15, BiomeType::HiveGlade, BugClass::Mantis);

    for (auto& td : rawTerrains) {
        td.id = (int)m_Terrains.size();
        switch(td.bugClass) {
            case BugClass::Dragonflies:   td.altitude = 3; break;
            case BugClass::Beetles:       td.altitude = 3; break;
            case BugClass::Termites:      td.altitude = 3; break;
            case BugClass::BeesWasps:     td.altitude = 3; break;
            case BugClass::Snails:        td.altitude = 2; break;
            case BugClass::Bugs:          td.altitude = 2; break;
            case BugClass::Spiders:       td.altitude = 2; break;
            case BugClass::ButterfliesMoths: td.altitude = 2; break;
            case BugClass::MosquitosTicks: td.altitude = 1; break;
            case BugClass::Scorpions:     td.altitude = 1; break;
            case BugClass::Ants:          td.altitude = 1; break;
            case BugClass::Fireflies:     td.altitude = 1; break;
            case BugClass::Woodlice:      td.altitude = 0; break;
            case BugClass::Roaches:       td.altitude = 0; break;
            case BugClass::CentipedesWorms: td.altitude = 0; break;
            case BugClass::Mantis:        td.altitude = 0; break;
            default:                      td.altitude = 1; break;
        }

        switch(td.bugClass) {
            case BugClass::Snails:         td.color = glm::vec3(0.50f, 0.55f, 0.50f); break;
            case BugClass::MosquitosTicks: td.color = glm::vec3(0.40f, 0.10f, 0.15f); break;
            case BugClass::Dragonflies:    td.color = glm::vec3(0.60f, 0.75f, 0.40f); break;
            case BugClass::Woodlice:       td.color = glm::vec3(0.70f, 0.70f, 0.75f); break;
            case BugClass::Scorpions:      td.color = glm::vec3(0.85f, 0.40f, 0.20f); break;
            case BugClass::Beetles:        td.color = glm::vec3(0.45f, 0.40f, 0.35f); break;
            case BugClass::Roaches:        td.color = glm::vec3(0.80f, 0.75f, 0.65f); break;
            case BugClass::Bugs:           td.color = glm::vec3(0.85f, 0.85f, 0.20f); break;
            case BugClass::Ants:           td.color = glm::vec3(0.45f, 0.35f, 0.25f); break;
            case BugClass::Termites:       td.color = glm::vec3(0.80f, 0.70f, 0.50f); break;
            case BugClass::CentipedesWorms:td.color = glm::vec3(0.25f, 0.20f, 0.15f); break;
            case BugClass::Spiders:        td.color = glm::vec3(0.90f, 0.90f, 0.95f); break;
            case BugClass::BeesWasps:      td.color = glm::vec3(0.95f, 0.85f, 0.10f); break;
            case BugClass::ButterfliesMoths:td.color = glm::vec3(0.90f, 0.60f, 0.85f); break;
            case BugClass::Mantis:         td.color = glm::vec3(0.20f, 0.65f, 0.25f); break;
            case BugClass::Fireflies:      td.color = glm::vec3(0.30f, 0.85f, 0.80f); break;
            default:                       td.color = glm::vec3(0.5f); break;
        }

        // Generate sub-sites (plates) for the whole biome to share
        float subScale = 40.0f;
        int gridW = (int)(config.width / subScale) + 2;
        int gridH = (int)(config.height / subScale) + 2;
        for(int gy = -1; gy < gridH; ++gy) {
            for(int gx = -1; gx < gridW; ++gx) {
                float bx = gx * subScale;
                float by = gy * subScale;
                std::mt19937 prng(gx * 1000 + gy + config.seed + (int)m_Terrains.size() * 555);
                std::uniform_real_distribution<float> fdist(0.1f, 0.9f);
                td.subSites.push_back(glm::vec2(bx + fdist(prng) * subScale, by + fdist(prng) * subScale));
            }
        }

        td.spawnPoint = td.site;
        m_Terrains.push_back(td);
    }

    m_Heightmap.resize(config.width * config.height);
    for (int y = 0; y < config.height; ++y) {
        for (int x = 0; x < config.width; ++x) {
            float fx = (float)x;
            float fy = (float)y;

            float warpScale = 0.005f;
            float warpIntensity = 60.0f;
            float wx = fx + (float)m_Perlin->octave2D(fx * warpScale, fy * warpScale, 2) * warpIntensity;
            float wy = fy + (float)m_Perlin->octave2D(fy * warpScale, fx * warpScale, 2) * warpIntensity;

            int s1_idx = -1;
            float d1_site = 1e10f;
            for (int i = 0; i < (int)m_Terrains.size(); ++i) {
                float dx = wx - m_Terrains[i].site.x;
                float dy = wy - m_Terrains[i].site.y;
                float d2 = dx*dx + dy*dy;
                if (d2 < d1_site) { d1_site = d2; s1_idx = i; }
            }

            if (s1_idx == -1) { m_Heightmap[y * config.width + x] = 0.0f; continue; }
            const auto& biomeSite = m_Terrains[s1_idx];
            float noiseVal = (float)m_Perlin->octave2D_01(fx * config.altitudeNoiseScale, fy * config.altitudeNoiseScale, 4);

            float finalH = 0.0f;
            switch(biomeSite.biomeType) {
                case BiomeType::Wetland:        finalH = GetWetlandHeight(fx, fy, biomeSite, noiseVal); break;
                case BiomeType::Desert:         finalH = GetDesertHeight(fx, fy, biomeSite, noiseVal); break;
                case BiomeType::MushroomForest: finalH = GetForestHeight(fx, fy, biomeSite, noiseVal); break;
                case BiomeType::HiveGlade:      finalH = GetGladeHeight(fx, fy, biomeSite, noiseVal); break;
                default: break;
            }
            m_Heightmap[y * config.width + x] = finalH;
        }
    }
}

float WorldManager::GetWetlandHeight(float x, float y, const TerrainData& td, float noiseVal) {
    float baseH = (float)td.altitude / (float)m_CurrentConfig.numAltitudeLevels;
    float angJitter = (float)m_Perlin->octave2D(x * 0.08, y * 0.08, 2) * 5.0f;
    glm::vec2 p(x + angJitter, y + angJitter);

    float dists[3] = {1e10f, 1e10f, 1e10f};
    for (const auto& ss : td.subSites) {
        float d = glm::distance(p, ss);
        if (d < dists[0]) { dists[2] = dists[1]; dists[1] = dists[0]; dists[0] = d; }
        else if (d < dists[1]) { dists[2] = dists[1]; dists[1] = d; }
        else if (d < dists[2]) { dists[2] = d; }
    }

    float edgeDist = (dists[1] - dists[0]);
    float plateStep = (edgeDist < 8.0f) ? 0.0f : 0.12f;
    float finalH = baseH + plateStep;

    double detailN = m_Perlin->octave2D_01(x * 0.1, y * 0.1, 2);
    if (td.bugClass == BugClass::MosquitosTicks) {
        if (detailN > 0.7) finalH -= 0.2f;
    } else if (td.bugClass == BugClass::Woodlice) {
        if (edgeDist < 4.0f) finalH += 0.05f;
    } else if (td.bugClass == BugClass::Dragonflies) {
        if (detailN > 0.6) finalH += 0.15f;
    }
    return std::clamp(finalH, 0.0f, 1.0f);
}

float WorldManager::GetDesertHeight(float x, float y, const TerrainData& td, float noiseVal) {
    return std::clamp((float)td.altitude / (float)m_CurrentConfig.numAltitudeLevels + noiseVal * 0.2f, 0.0f, 1.0f);
}

float WorldManager::GetForestHeight(float x, float y, const TerrainData& td, float noiseVal) {
    return std::clamp((float)td.altitude / (float)m_CurrentConfig.numAltitudeLevels + noiseVal * 0.2f, 0.0f, 1.0f);
}

float WorldManager::GetGladeHeight(float x, float y, const TerrainData& td, float noiseVal) {
    return std::clamp((float)td.altitude / (float)m_CurrentConfig.numAltitudeLevels + noiseVal * 0.2f, 0.0f, 1.0f);
}

void WorldManager::UpdateDebugTexture(SDL_GPUDevice* device, Texture** outTexture) {
    std::vector<unsigned char> pixels(m_CurrentConfig.width * m_CurrentConfig.height * 4);
    for (int y = 0; y < m_CurrentConfig.height; ++y) {
        for (int x = 0; x < m_CurrentConfig.width; ++x) {
            int pxIdx = (y * m_CurrentConfig.width + x) * 4;
            float h = m_Heightmap[y * m_CurrentConfig.width + x];
            if (m_CurrentConfig.showHeightmap) {
                unsigned char c = (unsigned char)(h * 255);
                pixels[pxIdx + 0] = c; pixels[pxIdx + 1] = c; pixels[pxIdx + 2] = c; pixels[pxIdx + 3] = 255;
            } else {
                int s1_idx = -1;
                float d1 = 1e10f;
                for (int i = 0; i < (int)m_Terrains.size(); ++i) {
                    float dx = (float)x - m_Terrains[i].site.x;
                    float dy = (float)y - m_Terrains[i].site.y;
                    float d2 = dx*dx + dy*dy;
                    if (d2 < d1) { d1 = d2; s1_idx = i; }
                }
                glm::vec3 color = m_Terrains[s1_idx].color * (0.7f + 0.3f * h);
                pixels[pxIdx + 0] = (unsigned char)(color.r * 255);
                pixels[pxIdx + 1] = (unsigned char)(color.g * 255);
                pixels[pxIdx + 2] = (unsigned char)(color.b * 255);
                pixels[pxIdx + 3] = 255;
            }
        }
    }
    if (*outTexture) delete *outTexture;
    *outTexture = new Texture(device, pixels.data(), m_CurrentConfig.width, m_CurrentConfig.height, TextureFilter::Nearest);
}
