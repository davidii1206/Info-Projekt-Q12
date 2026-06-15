#include "TerrainMeshBuilder.h"
#include "../Core/WorldManager.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>

namespace TerrainMeshBuilder {

namespace {

// Same neighbour direction tables as WorldManager::GenerateTileGrid -
// 0=+X, 1=-X, 2=+Z, 3=-Z. A Ramp tile's rampDir is the direction it descends.
constexpr int kDX[4] = { 1, -1, 0, 0 };
constexpr int kDZ[4] = { 0, 0, 1, -1 };
constexpr glm::vec3 kOutward[4] = {
    { 1.f, 0.f,  0.f }, { -1.f, 0.f,  0.f },
    { 0.f, 0.f,  1.f }, {  0.f, 0.f, -1.f }
};

glm::vec4 TierColor(int tier, int numTiers) {
    static constexpr glm::vec4 kPalette[4] = {
        { 0.80f, 0.74f, 0.55f, 1.f }, // tier 0: sand/shore
        { 0.42f, 0.58f, 0.27f, 1.f }, // tier 1: grass
        { 0.32f, 0.46f, 0.22f, 1.f }, // tier 2: dark grass
        { 0.55f, 0.52f, 0.50f, 1.f }, // tier 3: rock plateau
    };
    int idx = std::clamp(tier, 0, (int)(sizeof(kPalette) / sizeof(kPalette[0])) - 1);
    (void)numTiers;
    return kPalette[idx];
}

constexpr glm::vec4 kWaterColor = { 0.20f, 0.45f, 0.62f, 1.f };
constexpr glm::vec4 kCliffColor = { 0.40f, 0.37f, 0.35f, 1.f };
constexpr glm::vec4 kRampColor  = { 0.58f, 0.48f, 0.34f, 1.f };

void AddQuad(TerrainMeshData& out,
              const glm::vec3& v0, const glm::vec3& v1,
              const glm::vec3& v2, const glm::vec3& v3,
              const glm::vec3& normal, const glm::vec4& color)
{
    uint32_t base = (uint32_t)out.vertices.size();
    ModelVertex mv0{ v0, normal, {0.f, 0.f}, color };
    ModelVertex mv1{ v1, normal, {1.f, 0.f}, color };
    ModelVertex mv2{ v2, normal, {1.f, 1.f}, color };
    ModelVertex mv3{ v3, normal, {0.f, 1.f}, color };
    out.vertices.push_back(mv0);
    out.vertices.push_back(mv1);
    out.vertices.push_back(mv2);
    out.vertices.push_back(mv3);
    out.indices.push_back(base + 0);
    out.indices.push_back(base + 1);
    out.indices.push_back(base + 2);
    out.indices.push_back(base + 0);
    out.indices.push_back(base + 2);
    out.indices.push_back(base + 3);
}

} // namespace

TerrainMeshData Build(const WorldManager& world) {
    TerrainMeshData out;

    const auto& cfg = world.GetConfig();
    const int gridSize = world.GetGridSize();
    if (gridSize <= 0) return out;

    out.vertices.reserve((size_t)gridSize * gridSize * 8);
    out.indices.reserve((size_t)gridSize * gridSize * 12);

    for (int tz = 0; tz < gridSize; ++tz) {
        for (int tx = 0; tx < gridSize; ++tx) {
            const TerrainTile& tile = world.GetTile(tx, tz);
            glm::vec2 center = world.TileToWorld(tx, tz);
            const float half = cfg.tileSize * 0.5f;
            const float x0 = center.x - half, x1 = center.x + half;
            const float z0 = center.y - half, z1 = center.y + half;
            const float topH = world.TierToWorldHeight(tile.tier);

            // Corner heights: c0=(x0,z0) c1=(x1,z0) c2=(x1,z1) c3=(x0,z1).
            float h[4] = { topH, topH, topH, topH };
            glm::vec4 topColor;

            switch (tile.surface) {
                case TileSurface::Water:
                    topColor = kWaterColor;
                    break;
                case TileSurface::Cliff:
                    topColor = kCliffColor;
                    break;
                case TileSurface::Ramp: {
                    topColor = kRampColor;
                    const float lowH = topH - cfg.tierHeight;
                    // rampDir is the direction the tile descends towards.
                    switch (tile.rampDir) {
                        case 0: h[1] = lowH; h[2] = lowH; break; // +X edge low
                        case 1: h[0] = lowH; h[3] = lowH; break; // -X edge low
                        case 2: h[2] = lowH; h[3] = lowH; break; // +Z edge low
                        case 3: h[0] = lowH; h[1] = lowH; break; // -Z edge low
                        default: break;
                    }
                    break;
                }
                case TileSurface::Plateau:
                default:
                    topColor = TierColor(tile.tier, cfg.numTiers);
                    break;
            }

            glm::vec3 c0(x0, h[0], z0);
            glm::vec3 c1(x1, h[1], z0);
            glm::vec3 c2(x1, h[2], z1);
            glm::vec3 c3(x0, h[3], z1);

            glm::vec3 normal(0.f, 1.f, 0.f);
            if (tile.surface == TileSurface::Ramp) {
                normal = glm::normalize(glm::cross(c1 - c0, c3 - c0));
                if (normal.y < 0.f) normal = -normal;
            }

            AddQuad(out, c0, c1, c2, c3, normal, topColor);

            // --- Cliff walls towards lower neighbours ------------------------
            const glm::vec3 corners[4]    = { c0, c1, c2, c3 };
            // Edge d connects corners (edgeA[d], edgeB[d]).
            static constexpr int edgeA[4] = { 1, 3, 2, 0 };
            static constexpr int edgeB[4] = { 2, 0, 3, 1 };

            for (int d = 0; d < 4; ++d) {
                int nx = tx + kDX[d], nz = tz + kDZ[d];
                if (nx < 0 || nx >= gridSize || nz < 0 || nz >= gridSize) continue;

                const TerrainTile& nb = world.GetTile(nx, nz);
                const float neighborTopH = world.TierToWorldHeight(nb.tier);

                const glm::vec3& a = corners[edgeA[d]];
                const glm::vec3& b = corners[edgeB[d]];
                const float edgeMinH = std::min(a.y, b.y);
                if (edgeMinH <= neighborTopH + 1e-4f) continue; // no gap to fill

                glm::vec3 botA(a.x, neighborTopH, a.z);
                glm::vec3 botB(b.x, neighborTopH, b.z);
                AddQuad(out, a, botA, botB, b, kOutward[d], kCliffColor);
            }
        }
    }

    return out;
}

} // namespace TerrainMeshBuilder
