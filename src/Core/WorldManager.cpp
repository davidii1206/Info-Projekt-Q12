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
    std::vector<BugClass> classes = { BugClass::Ants, BugClass::Termites, BugClass::Spiders, BugClass::Woodlice };

    for (int i = 0; i < config.numTerrains; ++i) {
        TerrainData td;
        td.id = i;
        td.site = glm::vec2(points[i].x, points[i].y);
        td.bugClass = classes[i % classes.size()];
        
        double n = m_Perlin->octave2D_01(td.site.x * config.altitudeNoiseScale, td.site.y * config.altitudeNoiseScale, 2);
        td.altitude = (int)(n * config.numAltitudeLevels);
        
        switch(td.bugClass) {
            case BugClass::Ants:     td.color = glm::vec3(0.85f, 0.70f, 0.45f); break; 
            case BugClass::Termites: td.color = glm::vec3(0.70f, 0.50f, 0.35f); break; 
            case BugClass::Spiders:  td.color = glm::vec3(0.35f, 0.30f, 0.25f); break; 
            case BugClass::Woodlice: td.color = glm::vec3(0.55f, 0.60f, 0.65f); break; 
            default:                 td.color = glm::vec3(0.5f); break;
        }

        // Generate Sub-sites for Ant biome (pre-calculated to avoid per-pixel warping)
        if (td.bugClass == BugClass::Ants) {
            // Jittered grid with local density variations
            float subScale = 40.0f;
            int gridW = (int)(config.width / subScale) + 2;
            int gridH = (int)(config.height / subScale) + 2;
            
            for(int gy = -1; gy < gridH; ++gy) {
                for(int gx = -1; gx < gridW; ++gx) {
                    float bx = gx * subScale;
                    float by = gy * subScale;
                    
                    std::mt19937 prng(gx * 1000 + gy + config.seed + i * 555);
                    std::uniform_real_distribution<float> fdist(0.1f, 0.9f);
                    
                    // Use noise to decide if we split this cell into smaller ones
                    double densityN = m_Perlin->octave2D_01(bx * 0.01, by * 0.01, 2);
                    int splits = (densityN > 0.6) ? 2 : 1; 
                    
                    for(int sy = 0; sy < splits; ++sy) {
                        for(int sx = 0; sx < splits; ++sx) {
                            float px = bx + (sx + fdist(prng)) * (subScale / splits);
                            float py = by + (sy + fdist(prng)) * (subScale / splits);
                            td.subSites.push_back(glm::vec2(px, py));
                        }
                    }
                }
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

            int s1_idx = -1;
            float d1_site = 1e10f;
            for (int i = 0; i < (int)m_Terrains.size(); ++i) {
                float dx = fx - m_Terrains[i].site.x;
                float dy = fy - m_Terrains[i].site.y;
                float d2 = dx*dx + dy*dy;
                if (d2 < d1_site) { d1_site = d2; s1_idx = i; }
            }

            const auto& biome = m_Terrains[s1_idx];
            float finalH = 0.0f;

            if (biome.bugClass == BugClass::Ants) {
                // Add angular jitter to make straight lines look like they have more "low poly" vertices
                float angJitter = (float)m_Perlin->octave2D(fx * 0.08, fy * 0.08, 2) * 4.0f;
                glm::vec2 p(fx + angJitter, fy + angJitter);

                // Find closest 3 sub-sites
                float dists[3] = {1e10f, 1e10f, 1e10f};
                for (const auto& ss : biome.subSites) {
                    float d = glm::distance(p, ss);
                    if (d < dists[0]) { dists[2] = dists[1]; dists[1] = dists[0]; dists[0] = d; }
                    else if (d < dists[1]) { dists[2] = dists[1]; dists[1] = d; }
                    else if (d < dists[2]) { dists[2] = d; }
                }

                float edgeDist = (dists[1] - dists[0]);
                float vertexDist = (dists[2] - dists[1]);
                
                // Angular Plaza Factor: Use noise to make junctions have more "corners"
                float angle = std::atan2(fy - p.y, fx - p.x);
                float vertexJagged = (float)std::sin(angle * 6.0f + config.seed) * 2.0f;
                float plazaFactor = std::clamp(1.0f - (vertexDist + vertexJagged) * 0.15f, 0.0f, 1.0f);
                
                float noiseVal = (float)m_Perlin->octave2D_01(fx * 0.05, fy * 0.05, 2);
                float crackThreshold = 2.0f + 5.0f * noiseVal + plazaFactor * 12.0f; 
                
                finalH = (edgeDist < crackThreshold) ? 0.2f : 0.9f;

            } else {
                double n = m_Perlin->octave2D_01(fx * config.altitudeNoiseScale, fy * config.altitudeNoiseScale, 4);
                float levels = (float)(config.numAltitudeLevels > 1 ? config.numAltitudeLevels : 1);
                float rawValue = (float)n * levels;
                float floorVal = std::floor(rawValue);
                float fract = rawValue - floorVal;

                float slopeVar = (float)m_Perlin->octave2D_01(fx * 0.05, fy * 0.05, 2);
                float currentSharpness = config.slopeSharpness * (0.5f + 1.5f * slopeVar);
                float smoothFract = glm::smoothstep(0.5f - currentSharpness, 0.5f + currentSharpness, fract);
                float plateauH = (floorVal + smoothFract) / levels;

                float biomeH = 0.0f;
                double detailN = m_Perlin->octave2D_01(fx * 0.1, fy * 0.1, 2);
                switch(biome.bugClass) {
                    case BugClass::Termites: biomeH = (detailN > 0.7) ? 0.1f : 0.0f; break;
                    case BugClass::Spiders:  biomeH = (detailN < 0.2) ? -0.15f : 0.0f; break;
                    case BugClass::Woodlice: biomeH = (detailN > 0.9) ? 0.08f : 0.0f; break;
                    default: break;
                }
                finalH = std::clamp(plateauH * config.altitudeMaxHeight + biomeH * 0.03f, 0.0f, 1.0f);
            }

            m_Heightmap[y * config.width + x] = finalH;
        }
    }
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
