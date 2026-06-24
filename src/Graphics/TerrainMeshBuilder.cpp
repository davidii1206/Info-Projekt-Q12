#include "TerrainMeshBuilder.h"
#include "../Core/WorldManager.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cstdint>

namespace TerrainMeshBuilder {

namespace {

constexpr int kDX[4] = { 1, -1, 0, 0 };
constexpr int kDZ[4] = { 0, 0, 1, -1 };
constexpr glm::vec3 kOutward[4] = {
    { 1.f, 0.f,  0.f }, { -1.f, 0.f,  0.f },
    { 0.f, 0.f,  1.f }, {  0.f, 0.f, -1.f }
};

// Per-biome base colour, shifted darker at low tiers and lighter at high tiers.
glm::vec4 BiomeTierColor(BugClass bc, int tier, int numTiers) {
    glm::vec3 base;
    switch (bc) {
        case BugClass::Ants:             base = {0.82f, 0.68f, 0.44f}; break;
        case BugClass::Termites:         base = {0.30f, 0.44f, 0.16f}; break;
        case BugClass::Spiders:          base = {0.13f, 0.10f, 0.15f}; break;
        case BugClass::Woodlice:         base = {0.34f, 0.46f, 0.20f}; break;
        case BugClass::BeesWasps:        base = {0.38f, 0.52f, 0.20f}; break;
        case BugClass::ButterfliesMoths: base = {0.36f, 0.50f, 0.22f}; break;
        case BugClass::Snails:           base = {0.22f, 0.42f, 0.17f}; break;
        case BugClass::Mantis:           base = {0.17f, 0.38f, 0.12f}; break;
        case BugClass::Fireflies:        base = {0.20f, 0.36f, 0.20f}; break;
        case BugClass::CentipedesWorms:  base = {0.16f, 0.11f, 0.08f}; break;
        case BugClass::MosquitosTicks:   base = {0.27f, 0.39f, 0.14f}; break;
        case BugClass::Dragonflies:      base = {0.22f, 0.44f, 0.26f}; break;
        case BugClass::Bugs:             base = {0.42f, 0.52f, 0.13f}; break;
        case BugClass::Roaches:          base = {0.28f, 0.25f, 0.16f}; break;
        case BugClass::Beetles:          base = {0.22f, 0.20f, 0.21f}; break;
        case BugClass::Scorpions:        base = {0.90f, 0.78f, 0.52f}; break;
        case BugClass::BossArena:        base = {0.30f, 0.27f, 0.20f}; break;
        default:                          base = {0.40f, 0.56f, 0.26f}; break;
    }
    float t = (numTiers > 1) ? (float)tier / (float)(numTiers - 1) : 0.5f;
    glm::vec3 c = glm::clamp(glm::mix(base * 0.70f, base * 1.30f, t), 0.f, 0.95f);
    return glm::vec4(c, 1.f);
}

constexpr glm::vec4 kWaterColor = { 0.20f, 0.45f, 0.62f, 1.f };
constexpr glm::vec4 kCliffColor = { 0.40f, 0.37f, 0.35f, 1.f };
constexpr glm::vec4 kRampColor  = { 0.58f, 0.48f, 0.34f, 1.f };

constexpr float kJitterAmp  = 0.12f;

float CornerNoise(int cx, int cz) {
    uint32_t h = (uint32_t)cx * 0x9E3779B1u ^ (uint32_t)cz * 0x85EBCA77u;
    h ^= h >> 16; h *= 0x45d9f3bu; h ^= h >> 16;
    return (float)(h & 0xFFFF) / 32767.5f - 1.f;
}

float SmoothColorNoise(float x, float y, int seed) {
    auto H = [seed](int ix, int iy) -> float {
        uint32_t h = ((uint32_t)(ix + seed * 1031)) * 0x9E3779B1u
                   ^ ((uint32_t)(iy + seed * 2053)) * 0x85EBCA77u;
        h ^= h >> 16; h *= 0x45d9f3bu; h ^= h >> 16;
        return (float)(h & 0xFFFF) / 32767.5f - 1.f;
    };
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3.f - 2.f * fx);
    fy = fy * fy * (3.f - 2.f * fy);
    return glm::mix(glm::mix(H(ix, iy), H(ix+1, iy), fx),
                    glm::mix(H(ix, iy+1), H(ix+1, iy+1), fx), fy);
}

void AddTriangle(TerrainMeshData& out,
                 const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                 const glm::vec3& normal, const glm::vec4& color)
{
    uint32_t base = (uint32_t)out.vertices.size();
    out.vertices.push_back({ v0, normal, {0.f, 0.f}, color });
    out.vertices.push_back({ v1, normal, {1.f, 0.f}, color });
    out.vertices.push_back({ v2, normal, {0.5f, 1.f}, color });
    out.indices.push_back(base + 0);
    out.indices.push_back(base + 1);
    out.indices.push_back(base + 2);
}

void AddQuad(TerrainMeshData& out,
             const glm::vec3& v0, const glm::vec3& v1,
             const glm::vec3& v2, const glm::vec3& v3,
             const glm::vec3& normal, const glm::vec4& color)
{
    uint32_t base = (uint32_t)out.vertices.size();
    out.vertices.push_back({ v0, normal, {0.f, 0.f}, color });
    out.vertices.push_back({ v1, normal, {1.f, 0.f}, color });
    out.vertices.push_back({ v2, normal, {1.f, 1.f}, color });
    out.vertices.push_back({ v3, normal, {0.f, 1.f}, color });
    out.indices.push_back(base + 0);
    out.indices.push_back(base + 1);
    out.indices.push_back(base + 2);
    out.indices.push_back(base + 0);
    out.indices.push_back(base + 2);
    out.indices.push_back(base + 3);
}

void AddQuadVC(TerrainMeshData& out,
               const glm::vec3& v0, const glm::vec3& v1,
               const glm::vec3& v2, const glm::vec3& v3,
               const glm::vec3& normal,
               const glm::vec4& c0, const glm::vec4& c1,
               const glm::vec4& c2, const glm::vec4& c3)
{
    uint32_t base = (uint32_t)out.vertices.size();
    out.vertices.push_back({ v0, normal, {0.f, 0.f}, c0 });
    out.vertices.push_back({ v1, normal, {1.f, 0.f}, c1 });
    out.vertices.push_back({ v2, normal, {1.f, 1.f}, c2 });
    out.vertices.push_back({ v3, normal, {0.f, 1.f}, c3 });
    out.indices.push_back(base + 0);
    out.indices.push_back(base + 1);
    out.indices.push_back(base + 2);
    out.indices.push_back(base + 0);
    out.indices.push_back(base + 2);
    out.indices.push_back(base + 3);
}

// -----------------------------------------------------------------------
// Computes per-corner blended biome colours for the full grid.
// Shared by both Build() and BuildChunks().
// -----------------------------------------------------------------------
struct CornerColors {
    std::vector<glm::vec4> colors;
    int cornerW = 0;
};

CornerColors ComputeCornerColors(const WorldManager& world) {
    const auto& cfg = world.GetConfig();
    const int gridSize = world.GetGridSize();
    const auto& terrains = world.GetTerrains();

    CornerColors cc;
    cc.cornerW = gridSize + 1;
    cc.colors.assign((size_t)cc.cornerW * cc.cornerW, {0.f, 0.f, 0.f, 0.f});
    std::vector<int> count((size_t)cc.cornerW * cc.cornerW, 0);

    for (int tz = 0; tz < gridSize; ++tz) {
        for (int tx = 0; tx < gridSize; ++tx) {
            const TerrainTile& tile = world.GetTile(tx, tz);
            if (tile.surface != TileSurface::Plateau) continue;

            BugClass bc = (tile.territoryId < terrains.size())
                ? terrains[tile.territoryId].bugClass : BugClass::None;
            glm::vec4 col = BiomeTierColor(bc, tile.tier, cfg.numTiers);

            const int cxs[4] = { tx, tx+1, tx+1, tx   };
            const int czs[4] = { tz, tz,   tz+1, tz+1 };
            for (int i = 0; i < 4; ++i) {
                int idx = czs[i] * cc.cornerW + cxs[i];
                cc.colors[idx] += col;
                count[idx]++;
            }
        }
    }
    for (int i = 0; i < cc.cornerW * cc.cornerW; ++i) {
        if (count[i] > 0)
            cc.colors[i] /= (float)count[i];
        else
            cc.colors[i] = BiomeTierColor(BugClass::None, 0, cfg.numTiers);

        int cx = i % cc.cornerW, cz = i / cc.cornerW;
        float n = SmoothColorNoise((float)cx / 12.f, (float)cz / 12.f, 0) * 0.10f
                + SmoothColorNoise((float)cx /  5.f, (float)cz /  5.f, 3) * 0.04f;
        float b = 1.f + n;
        cc.colors[i] = glm::clamp(
            glm::vec4(cc.colors[i].r * b, cc.colors[i].g * b, cc.colors[i].b * b, 1.f),
            0.f, 0.95f);
    }
    return cc;
}

// -----------------------------------------------------------------------
// Builds geometry for tiles in [txStart, txEnd) × [tzStart, tzEnd).
// Used by both Build() and BuildChunks().
// -----------------------------------------------------------------------
void BuildTileRange(TerrainMeshData& out,
                    const WorldManager& world,
                    int txStart, int txEnd,
                    int tzStart, int tzEnd,
                    const CornerColors& cc)
{
    const auto& cfg = world.GetConfig();
    const int gridSize = world.GetGridSize();
    const auto& terrains = world.GetTerrains();

    for (int tz = tzStart; tz < tzEnd; ++tz) {
        for (int tx = txStart; tx < txEnd; ++tx) {
            const TerrainTile& tile = world.GetTile(tx, tz);
            glm::vec2 center = world.TileToWorld(tx, tz);
            const float half = cfg.tileSize * 0.5f;
            const float x0 = center.x - half, x1 = center.x + half;
            const float z0 = center.y - half, z1 = center.y + half;
            const float topH = world.TierToWorldHeight(tile.tier);

            float h[4] = { topH, topH, topH, topH };
            glm::vec4 topColor;
            bool useFlatColor = true;

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
                    switch (tile.rampDir) {
                        case 0: h[1] = lowH; h[2] = lowH; break;
                        case 1: h[0] = lowH; h[3] = lowH; break;
                        case 2: h[2] = lowH; h[3] = lowH; break;
                        case 3: h[0] = lowH; h[1] = lowH; break;
                        default: break;
                    }
                    break;
                }
                case TileSurface::Plateau:
                default:
                    useFlatColor = false;
                    break;
            }

            const float hBase[4] = { h[0], h[1], h[2], h[3] };

            if (tile.surface == TileSurface::Plateau) {
                h[0] += CornerNoise(tx,   tz  ) * kJitterAmp;
                h[1] += CornerNoise(tx+1, tz  ) * kJitterAmp;
                h[2] += CornerNoise(tx+1, tz+1) * kJitterAmp;
                h[3] += CornerNoise(tx,   tz+1) * kJitterAmp;
            }

            glm::vec3 c0(x0, h[0], z0);
            glm::vec3 c1(x1, h[1], z0);
            glm::vec3 c2(x1, h[2], z1);
            glm::vec3 c3(x0, h[3], z1);

            glm::vec3 normal(0.f, 1.f, 0.f);
            if (tile.surface == TileSurface::Ramp || tile.surface == TileSurface::Plateau) {
                normal = glm::normalize(glm::cross(c1 - c0, c3 - c0));
                if (normal.y < 0.f) normal = -normal;
            }

            if (useFlatColor) {
                AddQuad(out, c0, c1, c2, c3, normal, topColor);

                if (tile.surface == TileSurface::Ramp) {
                    const glm::vec4 wallColor = glm::clamp(kRampColor * 0.80f, 0.f, 1.f);
                    auto noRampNeighbour = [&](int nx, int nz) -> bool {
                        if (nx < 0 || nx >= gridSize || nz < 0 || nz >= gridSize) return true;
                        const TerrainTile& nb = world.GetTile(nx, nz);
                        return !(nb.surface == TileSurface::Ramp && nb.rampDir == tile.rampDir);
                    };
                    switch (tile.rampDir) {
                        case 0: {
                            glm::vec3 w0(x1, topH, z0), w1(x1, topH, z1);
                            if (noRampNeighbour(tx, tz-1)) AddTriangle(out, c0, w0, c1, {0.f, 0.f, -1.f}, wallColor);
                            if (noRampNeighbour(tx, tz+1)) AddTriangle(out, c3, c2, w1, {0.f, 0.f,  1.f}, wallColor);
                            break;
                        }
                        case 1: {
                            glm::vec3 w0(x0, topH, z0), w1(x0, topH, z1);
                            if (noRampNeighbour(tx, tz-1)) AddTriangle(out, c1, c0, w0, {0.f, 0.f, -1.f}, wallColor);
                            if (noRampNeighbour(tx, tz+1)) AddTriangle(out, c2, w1, c3, {0.f, 0.f,  1.f}, wallColor);
                            break;
                        }
                        case 2: {
                            glm::vec3 w0(x0, topH, z1), w1(x1, topH, z1);
                            if (noRampNeighbour(tx-1, tz)) AddTriangle(out, c0, c3, w0, {-1.f, 0.f, 0.f}, wallColor);
                            if (noRampNeighbour(tx+1, tz)) AddTriangle(out, c1, w1, c2, { 1.f, 0.f, 0.f}, wallColor);
                            break;
                        }
                        case 3: {
                            glm::vec3 w0(x0, topH, z0), w1(x1, topH, z0);
                            if (noRampNeighbour(tx-1, tz)) AddTriangle(out, c3, w0, c0, {-1.f, 0.f, 0.f}, wallColor);
                            if (noRampNeighbour(tx+1, tz)) AddTriangle(out, c2, c1, w1, { 1.f, 0.f, 0.f}, wallColor);
                            break;
                        }
                        default: break;
                    }
                }
            } else {
                glm::vec4 cc0 = cc.colors[tz       * cc.cornerW + tx    ];
                glm::vec4 cc1 = cc.colors[tz       * cc.cornerW + (tx+1)];
                glm::vec4 cc2 = cc.colors[(tz + 1) * cc.cornerW + (tx+1)];
                glm::vec4 cc3 = cc.colors[(tz + 1) * cc.cornerW + tx    ];
                AddQuadVC(out, c0, c1, c2, c3, normal, cc0, cc1, cc2, cc3);
            }

            // --- Cliff walls ---
            const glm::vec3 corners[4] = { c0, c1, c2, c3 };
            static constexpr int edgeA[4] = { 1, 3, 2, 0 };
            static constexpr int edgeB[4] = { 2, 0, 3, 1 };
            const int cornerGX[4] = { tx, tx+1, tx+1, tx   };
            const int cornerGZ[4] = { tz, tz,   tz+1, tz+1 };

            for (int d = 0; d < 4; ++d) {
                int nx = tx + kDX[d], nz = tz + kDZ[d];
                if (nx < 0 || nx >= gridSize || nz < 0 || nz >= gridSize) continue;

                const TerrainTile& nb = world.GetTile(nx, nz);
                const float neighborTopH = world.TierToWorldHeight(nb.tier);

                const float edgeMinH = std::min(hBase[edgeA[d]], hBase[edgeB[d]]);
                if (edgeMinH <= neighborTopH + 1e-4f) continue;

                const glm::vec3& a = corners[edgeA[d]];
                const glm::vec3& b = corners[edgeB[d]];

                const bool nbIsPlat = (nb.surface == TileSurface::Plateau);
                const float jA = nbIsPlat ? CornerNoise(cornerGX[edgeA[d]], cornerGZ[edgeA[d]]) * kJitterAmp : 0.f;
                const float jB = nbIsPlat ? CornerNoise(cornerGX[edgeB[d]], cornerGZ[edgeB[d]]) * kJitterAmp : 0.f;
                const float botHA = neighborTopH + jA;
                const float botHB = neighborTopH + jB;

                glm::vec3 botA(a.x, botHA + 0.01f, a.z);
                glm::vec3 botB(b.x, botHB + 0.01f, b.z);

                uint32_t wh = (uint32_t)tx * 0x9E3779B1u ^ (uint32_t)tz * 0x85EBCA77u
                            ^ (uint32_t)d  * 0x19349663u;
                wh ^= wh >> 16; wh *= 0x45d9f3bu; wh ^= wh >> 16;
                float segVar = 0.78f + (float)(wh & 0xFF) / 255.f * 0.44f;

                const glm::vec3 cliffNorm = kOutward[d];

                const float midHA = (a.y + botHA) * 0.5f;
                const float midHB = (b.y + botHB) * 0.5f;
                glm::vec3 midA(a.x, midHA, a.z);
                glm::vec3 midB(b.x, midHB, b.z);

                glm::vec4 colTop(kCliffColor.r * segVar * 1.15f,
                                 kCliffColor.g * segVar * 1.15f,
                                 kCliffColor.b * segVar * 1.15f, 1.f);
                glm::vec4 colBot(kCliffColor.r * segVar * 0.70f,
                                 kCliffColor.g * segVar * 0.70f,
                                 kCliffColor.b * segVar * 0.70f, 1.f);
                colTop = glm::clamp(colTop, 0.f, 1.f);
                colBot = glm::clamp(colBot, 0.f, 1.f);

                AddQuad(out, a,    midA, midB, b,    cliffNorm, colTop);
                AddQuad(out, midA, botA, botB, midB, cliffNorm, colBot);
            }
        }
    }
}

} // namespace

TerrainMeshData Build(const WorldManager& world) {
    TerrainMeshData out;
    const int gridSize = world.GetGridSize();
    if (gridSize <= 0) return out;

    out.vertices.reserve((size_t)gridSize * gridSize * 8);
    out.indices.reserve((size_t)gridSize * gridSize * 12);

    CornerColors cc = ComputeCornerColors(world);
    BuildTileRange(out, world, 0, gridSize, 0, gridSize, cc);

    return out;
}

ChunkBuildResult BuildChunks(const WorldManager& world, int chunkSize) {
    ChunkBuildResult result;
    const int gridSize = world.GetGridSize();
    if (gridSize <= 0) return result;

    result.chunkSize = chunkSize;
    result.chunksPerAxis = (gridSize + chunkSize - 1) / chunkSize;
    result.chunks.resize((size_t)result.chunksPerAxis * result.chunksPerAxis);

    CornerColors cc = ComputeCornerColors(world);

    for (int cz = 0; cz < result.chunksPerAxis; ++cz) {
        for (int cx = 0; cx < result.chunksPerAxis; ++cx) {
            int txStart = cx * chunkSize;
            int txEnd   = std::min(txStart + chunkSize, gridSize);
            int tzStart = cz * chunkSize;
            int tzEnd   = std::min(tzStart + chunkSize, gridSize);

            TerrainMeshData& out = result.chunks[cz * result.chunksPerAxis + cx];
            out.vertices.reserve((size_t)chunkSize * chunkSize * 8);
            out.indices.reserve((size_t)chunkSize * chunkSize * 12);

            BuildTileRange(out, world, txStart, txEnd, tzStart, tzEnd, cc);
        }
    }

    return result;
}

} // namespace TerrainMeshBuilder
