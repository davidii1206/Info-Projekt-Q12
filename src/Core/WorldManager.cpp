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

    // -----------------------------------------------------------------------
    // Territory layout: Voronoi + Lloyd relaxation. (KEEP — see WORLDGEN_PLAN)
    // -----------------------------------------------------------------------
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

    // Faction round-robin across all 16 playable types. NOTE: this is a
    // placeholder territory tag for the debug view only — terrain is neutral;
    // faction ownership is a gameplay property assigned elsewhere.
    static const BugClass kAllFactions[] = {
        BugClass::BeesWasps, BugClass::ButterfliesMoths, BugClass::Snails,
        BugClass::Mantis, BugClass::Spiders, BugClass::Fireflies, BugClass::Ants,
        BugClass::Termites, BugClass::CentipedesWorms, BugClass::MosquitosTicks,
        BugClass::Woodlice, BugClass::Dragonflies, BugClass::Bugs, BugClass::Roaches,
        BugClass::Beetles, BugClass::Scorpions
    };

    for (int i = 0; i < config.numTerrains; ++i) {
        TerrainData td;
        td.id = i;
        td.site = glm::vec2(points[i].x, points[i].y);
        td.bugClass = kAllFactions[i % 16];

        switch (td.bugClass) {
            case BugClass::Ants:     td.color = glm::vec3(0.85f, 0.70f, 0.45f); break;
            case BugClass::Termites: td.color = glm::vec3(0.70f, 0.50f, 0.35f); break;
            case BugClass::Spiders:  td.color = glm::vec3(0.35f, 0.30f, 0.25f); break;
            case BugClass::Woodlice: td.color = glm::vec3(0.55f, 0.60f, 0.65f); break;
            default:                 td.color = glm::vec3(0.5f);                break;
        }

        td.spawnPoint = td.site;
        m_Terrains.push_back(td);
    }

    // -----------------------------------------------------------------------
    // TEMPORARY flat heightmap.
    //
    // The organic per-pixel heightmap generator (smoothstep slopes, Ant-biome
    // jagged-plaza sub-sites, per-biome height nudges) was removed during the
    // worldgen teardown — it contradicts the terraced Thronefall model.
    // We fill a flat (all-zero) heightmap so ScatterSystem keeps working until
    // the TerrainTile grid + vertex-displaced mesh replace it.
    // See docs/WORLDGEN_PLAN.md.
    // -----------------------------------------------------------------------
    m_Heightmap.assign((size_t)config.width * config.height, 0.0f);
}

void WorldManager::UpdateDebugTexture(SDL_GPUDevice* device, Texture** outTexture) {
    // Territory map preview: flat per-cell faction colour (nearest Voronoi
    // site). The heightmap-grayscale branch was removed with the organic
    // generator; this will move to flat tier colour bands once the
    // TerrainTile grid lands. See docs/WORLDGEN_PLAN.md.
    std::vector<unsigned char> pixels(m_CurrentConfig.width * m_CurrentConfig.height * 4);
    for (int y = 0; y < m_CurrentConfig.height; ++y) {
        for (int x = 0; x < m_CurrentConfig.width; ++x) {
            int pxIdx = (y * m_CurrentConfig.width + x) * 4;
            int s1_idx = -1;
            float d1 = 1e10f;
            for (int i = 0; i < (int)m_Terrains.size(); ++i) {
                float dx = (float)x - m_Terrains[i].site.x;
                float dy = (float)y - m_Terrains[i].site.y;
                float d2 = dx*dx + dy*dy;
                if (d2 < d1) { d1 = d2; s1_idx = i; }
            }
            glm::vec3 color = (s1_idx >= 0) ? m_Terrains[s1_idx].color : glm::vec3(0.f);
            pixels[pxIdx + 0] = (unsigned char)(color.r * 255);
            pixels[pxIdx + 1] = (unsigned char)(color.g * 255);
            pixels[pxIdx + 2] = (unsigned char)(color.b * 255);
            pixels[pxIdx + 3] = 255;
        }
    }
    if (*outTexture) delete *outTexture;
    *outTexture = new Texture(device, pixels.data(), m_CurrentConfig.width, m_CurrentConfig.height, TextureFilter::Nearest);
}
