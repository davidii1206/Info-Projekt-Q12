/**
 * @file GameScene.cpp
 * @brief Implementation of the main gameplay scene.
 */

#define GLM_ENABLE_EXPERIMENTAL
#include "GameScene.h"
#include "LobbyScene.h"
#include "MainMenuScene.h"
#include "World.h"
#include "Components.h"
#include "Systems.h"
#include "../Networking/NetworkManager.h"
#include "../Networking/Packets.h"
#include "../Core/Input.h"
#include "../Core/AssetManager.h"
#include "../Core/MeshCollisionBuilder.h"
#include "../Core/DebugUI.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/TerrainMeshBuilder.h"
#include "StructurePlacementSystem.h"
#include "ResourceTypes.h"
#include "ResourceHUD.h"
#include "BuildingSystem.h"
#include "FogOfWar.h"
#include "TerritorySystem.h"
#include "HUDTextureRegistry.h"
#include "../Graphics/API/Shader.h"
#include "../Graphics/API/GraphicsPipeline.h"
#include "../Graphics/API/Framebuffer.h"
#include "../Graphics/API/GPUBuffer.h"
#include "../Graphics/Lights.h"
#include "PostProcessor.h"
#include <PerlinNoise.hpp>
#include "Pathfinding.h"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>
#include <cstring>
#include <random>
#include <algorithm>

// ---------------------------------------------------------------------------
// Terrain placement helpers
//
// The building/unit systems were written for a flat Y=0 map. These helpers
// let placement and spawning query the terraced worldgen terrain so props
// sit on the ground and can only be built on valid plateau tiles.
// ---------------------------------------------------------------------------
namespace {

/// World-space ground height (terrace top) at the given world XZ.
float GroundHeightAt(const WorldManager& world, float wx, float wz) {
    int tx, tz;
    world.WorldToTile(wx, wz, tx, tz);
    return world.TierToWorldHeight(world.GetTile(tx, tz).tier);
}

/// True if a building may be placed at the given world XZ:
/// a flat, dry, interior plateau tile (WorldManager::buildable flag).
bool IsBuildableAt(const WorldManager& world, float wx, float wz) {
    if (world.GetGridSize() <= 0) return false;
    int tx, tz;
    world.WorldToTile(wx, wz, tx, tz);
    return world.GetTile(tx, tz).buildable;
}

/// True if a unit may walk onto the tile at the given world XZ.
/// Less strict than IsBuildableAt: ramps are allowed (units climb corridor
/// ramps); only Water and Cliff faces are forbidden. The optional
/// @p currentTier guards against climbing onto tiles more than one tier
/// higher (i.e. off-corridor cliff walls).
bool IsWalkableAt(const WorldManager& world, float wx, float wz, int currentTier = -1, bool isFlying = false, bool isClimber = false) {
    if (isFlying) return true;
    if (world.GetGridSize() <= 0) return false;
    int tx, tz;
    world.WorldToTile(wx, wz, tx, tz);
    const auto& tile = world.GetTile(tx, tz);
    if (tile.surface == TileSurface::Water) return false;
    if (!isClimber) {
        if (tile.surface == TileSurface::Cliff) return false;
        if (currentTier >= 0 && (int)tile.tier - currentTier > 1) return false;
    }
    return true;
}

/// True if a scatter model path corresponds to a tall asset (tree, cactus,
/// mushroom, bamboo) that can occlude the isometric view in building mode.
bool IsTreeModelPath(const std::string& path) {
    // Trees, willows, pines, birches (live and dead), cacti, mushrooms, bamboo
    return path.find("Tree")    != std::string::npos ||
           path.find("Willow")  != std::string::npos ||
           path.find("Pine")    != std::string::npos ||
           path.find("Birch")   != std::string::npos ||
           path.find("Cactus")  != std::string::npos ||
           path.find("Mushroom")!= std::string::npos ||
           path.find("Bamboo")  != std::string::npos;
}

// ---------------------------------------------------------------------------
// Frustum culling helpers
// ---------------------------------------------------------------------------

/// One frustum plane in Hessian normal form (normal · point + distance = 0).
struct FrustumPlane {
    glm::vec3 normal{0.f};
    float     distance = 0.f;
};

/// Extracts 6 frustum planes from a view-projection matrix (Gribb-Hartmann).
/// Planes order: Left, Right, Bottom, Top, Near, Far.
/// Normals point inward (inside the frustum).
static void ExtractFrustumPlanes(const glm::mat4& vp, FrustumPlane* outPlanes) {
    for (int i = 0; i < 6; ++i) {
        int r = i >> 1;           // 0,0,1,1,2,2
        int s = (i & 1) ? 1 : -1; // -1,+1,-1,+1,-1,+1
        outPlanes[i].normal = glm::vec3(
            vp[0][3] + (float)s * vp[0][r],
            vp[1][3] + (float)s * vp[1][r],
            vp[2][3] + (float)s * vp[2][r]
        );
        outPlanes[i].distance = vp[3][3] + (float)s * vp[3][r];
        float len = glm::length(outPlanes[i].normal);
        if (len > 1e-8f) {
            outPlanes[i].normal   /= len;
            outPlanes[i].distance /= len;
        }
    }
}

/// Tests an AABB against the 6 frustum planes (p-vertex test).
/// Returns false if the box is completely outside (culled).
static bool IsAABBVisible(const glm::vec3& aabbMin, const glm::vec3& aabbMax,
                          const FrustumPlane* planes) {
    for (int i = 0; i < 6; ++i) {
        const auto& p = planes[i];
        glm::vec3 pos = aabbMin;
        if (p.normal.x >= 0.f) pos.x = aabbMax.x;
        if (p.normal.y >= 0.f) pos.y = aabbMax.y;
        if (p.normal.z >= 0.f) pos.z = aabbMax.z;
        if (glm::dot(pos, p.normal) + p.distance < 0.f)
            return false;
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

GameScene::GameScene() {}
GameScene::~GameScene() {}

// ---------------------------------------------------------------------------
// GPU instancing helpers
// ---------------------------------------------------------------------------

void GameScene::BuildScatterBatches(SceneContext& ctx, const FogGrid* fog) {
    m_ScatterBatches.clear();

    SDL_GPUDevice* device = ctx.renderer->GetDevice();

    // Pass 1: collect entity world matrices per model path.
    std::unordered_map<std::string, std::vector<glm::mat4>> entityMatsByPath;
    auto scatterView = ctx.clientRegistry.view<TransformComponent, ModelComponent, ScatterPropComponent>();
    for (auto entity : scatterView) {
        auto& tf = scatterView.get<TransformComponent>(entity);
        auto& mc = scatterView.get<ModelComponent>(entity);

        // Fog of war culling: skip scatters in unrevealed cells
        if (fog && fog->IsInitialised() && !fog->IsWorldPosRevealed(tf.position))
            continue;

        glm::mat4 entityMat =
            glm::translate(glm::mat4(1.f), tf.position) *
            glm::mat4_cast(glm::quat(glm::radians(tf.rotation))) *
            glm::scale(glm::mat4(1.f), tf.scale);
        entityMatsByPath[mc.modelPath].push_back(entityMat);
    }

    // Pass 2: for each model, build one SSBO per GLTF mesh node.
    // Each SSBO entry = entityMat * meshNode.transform (full world matrix for that instance).
    for (auto& [path, entityMats] : entityMatsByPath) {
        const auto& sceneData = AssetManager::LoadGLTF(path);
        if (!sceneData.model || sceneData.meshInstances.empty()) continue;

        for (uint32_t meshIdx = 0; meshIdx < (uint32_t)sceneData.meshInstances.size(); ++meshIdx) {
            const glm::mat4& nodeTransform = sceneData.meshInstances[meshIdx].transform;

            std::vector<glm::mat4> transforms;
            transforms.reserve(entityMats.size());
            for (const auto& em : entityMats)
                transforms.push_back(em * nodeTransform);

            uint32_t bufSize = (uint32_t)(transforms.size() * sizeof(glm::mat4));
            auto buf = std::make_unique<GPUBuffer>(device, BufferUsage::Uniform, bufSize);
            buf->Upload(transforms.data(), bufSize);

            ScatterBatch batch;
            batch.modelPath      = path;
            batch.meshInstanceIdx = meshIdx;
            batch.instanceCount  = (uint32_t)transforms.size();
            batch.instanceBuffer = std::move(buf);
            m_ScatterBatches.push_back(std::move(batch));
        }
    }

    // Identity instance buffer: single mat4(1) used for all non-scatter draw calls
    // so that the instance SSBO slot in the shader is always bound.
    glm::mat4 identity(1.f);
    m_IdentityInstanceBuffer = std::make_unique<GPUBuffer>(device, BufferUsage::Uniform, sizeof(glm::mat4));
    m_IdentityInstanceBuffer->Upload(&identity, sizeof(glm::mat4));

    spdlog::info("GameScene: built {} scatter batches ({} unique models × mesh nodes)",
                 m_ScatterBatches.size(), entityMatsByPath.size());
}

const FogGrid& GameScene::LocalFog(const SceneContext& ctx) const
{
    if (ctx.network.IsHosting()) {
        auto it = m_TeamFogs.find(m_MyPlayerId);
        if (it != m_TeamFogs.end()) return it->second;
        if (!m_TeamFogs.empty()) return m_TeamFogs.begin()->second;
    }
    return m_ClientFog;
}

bool GameScene::IsChunkRevealed(const FogGrid& fog, int chunkX, int chunkZ) const
{
    int txStart = chunkX * m_ChunkSize;
    int tzStart = chunkZ * m_ChunkSize;
    int txEnd = std::min(txStart + m_ChunkSize, m_World.GetGridSize());
    int tzEnd = std::min(tzStart + m_ChunkSize, m_World.GetGridSize());

    // Check a sparse grid of sample points across the chunk.
    // Step by 4 tiles to keep the check fast; this is conservative
    // (may over-cull at the fog edge by one row of tiles).
    constexpr int kStep = 4;
    for (int tz = tzStart; tz < tzEnd; tz += kStep) {
        for (int tx = txStart; tx < txEnd; tx += kStep) {
            glm::vec2 worldPos = m_World.TileToWorld(tx, tz);
            if (fog.IsWorldPosRevealed(glm::vec3(worldPos.x, 0.f, worldPos.y)))
                return true;
        }
    }
    return false;
}

void GameScene::GenerateMapTexture(SceneContext& ctx) {
    const FogGrid& fog = LocalFog(ctx);
    if (!fog.IsInitialised()) return;

    constexpr int TEX_SIZE = 512;
    const float worldMin = fog.worldMin.x; // -375
    const float worldMax = fog.worldMax.x; // +375
    const float worldSpan = worldMax - worldMin; // 750

    std::vector<uint8_t> pixels(TEX_SIZE * TEX_SIZE * 4, 0);

    // Pre-fetch territories for fast color lookup
    const auto& terrains = m_World.GetTerrains();

    for (int py = 0; py < TEX_SIZE; ++py) {
        for (int px = 0; px < TEX_SIZE; ++px) {
            // World position of this pixel (center of pixel area)
            float wx = worldMin + (px + 0.5f) / TEX_SIZE * worldSpan;
            float wz = worldMin + (py + 0.5f) / TEX_SIZE * worldSpan;

            // Fog check
            int fcx, fcz;
            fog.WorldToCell(glm::vec3(wx, 0, wz), fcx, fcz);
            if (!fog.IsRevealed(fcx, fcz)) {
                // Unexplored → black
                int idx = (py * TEX_SIZE + px) * 4;
                pixels[idx + 0] = 0;
                pixels[idx + 1] = 0;
                pixels[idx + 2] = 0;
                pixels[idx + 3] = 255;
                continue;
            }

            // Terrain tile lookup
            int tx, tz;
            m_World.WorldToTile(wx, wz, tx, tz);
            const auto& tile = m_World.GetTile(tx, tz);

            // Base color from territory or generic terrain
            float r = 0.3f, g = 0.5f, b = 0.2f; // default green
            if (tile.surface == TileSurface::Water) {
                r = 0.1f; g = 0.2f; b = 0.6f; // blue
            } else if (tile.territoryId < terrains.size()) {
                const auto& terr = terrains[tile.territoryId];
                r = terr.color.r;
                g = terr.color.g;
                b = terr.color.b;
            }

            // Height-based shading (higher = brighter)
            float heightFrac = (float)tile.tier / (float)m_World.GetConfig().numTiers;
            float shade = 0.6f + heightFrac * 0.4f;

            int idx = (py * TEX_SIZE + px) * 4;
            pixels[idx + 0] = (uint8_t)(std::min(r * shade, 1.0f) * 255);
            pixels[idx + 1] = (uint8_t)(std::min(g * shade, 1.0f) * 255);
            pixels[idx + 2] = (uint8_t)(std::min(b * shade, 1.0f) * 255);
            pixels[idx + 3] = 255;
        }
    }

    // Upload as GPU texture directly (bypass AssetManager cache since we regenerate)
    m_MapTexture = std::make_unique<Texture>(ctx.renderer->GetDevice(), pixels.data(), TEX_SIZE, TEX_SIZE);
    m_MapTextureDirty = false;
}

/**
 * @brief Initializes the camera and spawns a test asset if hosting.
 * @param ctx The scene context.
 */
void GameScene::OnEnter(SceneContext& ctx) {
    spdlog::info("GameScene: entered");
    UpgradeSystem::Reset();

    // ------------------------------------------------------------------
    // Recover identity from existing PlayerComponents (Lobby→Game).
    //
    // Without this, m_MyPlayerId / m_IdAssigned / m_NextNetId / m_NextPlayerId
    // / m_PeerToNetId all default to fresh values and any peer that's still
    // connected from the lobby session can't be matched to incoming packets,
    // and the next CONNECT event re-issues an already-used netId.
    // ------------------------------------------------------------------
    {
        uint32_t maxNetId    = 0;
        uint32_t maxPlayerId = 0;
        bool     anyPlayer   = false;

        auto pView = ctx.serverRegistry.view<PlayerComponent>();
        for (auto e : pView) {
            const auto& pc = pView.get<PlayerComponent>(e);
            auto* nc = ctx.serverRegistry.try_get<NetworkedComponent>(e);
            if (nc) m_ServerNetMap[nc->netId] = e;
            if (pc.peerId != 0xFFFFFFFFu && nc)
                m_PeerToNetId[pc.peerId] = nc->netId;
            anyPlayer = true;
            if (nc && nc->netId > maxNetId)  maxNetId    = nc->netId;
            if (pc.playerId > maxPlayerId)   maxPlayerId = pc.playerId;
        }
        if (anyPlayer) {
            m_NextNetId    = maxNetId + 1;
            m_NextPlayerId = maxPlayerId + 1;
        }

        // Mirror client-registry rebuild and recover local identity.
        auto cView = ctx.clientRegistry.view<PlayerComponent>();
        for (auto e : cView) {
            const auto& pc = cView.get<PlayerComponent>(e);
            auto* nc = ctx.clientRegistry.try_get<NetworkedComponent>(e);
            if (nc) m_ClientNetMap[nc->netId] = e;
            if (pc.isLocal) {
                m_MyPlayerId = pc.playerId;
                m_MyNetId    = nc ? nc->netId : 0;
                m_IdAssigned = true;
            }
        }
    }

    if (!m_Camera) {
        m_Camera = std::make_unique<Camera>();
        m_CameraMode = CameraMode::Building;
        m_Camera->SetProjectionMode(ProjectionMode::Orthographic);
        m_Camera->m_OrthoSize = m_BuildOrthoSize;
        m_Camera->m_Pitch = -55.f;
        m_Camera->m_Yaw   = m_BuildYaw;
        m_Camera->m_Position = glm::vec3(0.f, m_TopDownHeight, 0.f);
        m_Camera->UpdateVectors();
    }

    // ------------------------------------------------------------------
    // Procedural terrain (terraced tile grid, see docs/WORLDGEN_PLAN.md).
    //
    // Generated identically on every peer from the shared seed. The CPU
    // mesh is used both for rendering (registered with AssetManager under a
    // synthetic key so the existing ModelComponent render path picks it up)
    // and, on the host, for static physics collision.
    // ------------------------------------------------------------------
    TerrainMeshBuilder::TerrainMeshData terrainMesh;
    {
        WorldGenConfig genCfg;
        genCfg.seed        = m_WorldSeed;
        genCfg.worldExtent = 375.f;
        genCfg.numTiers       = 12;
        genCfg.tierHeight     = 1.2f;
        genCfg.tierNoiseScale = 0.010f;
        m_World.Generate(genCfg);

        // --- Procedural ground detail texture (shared by all chunk scenes) -----
        auto terrainDetailTex = [&]() {
            constexpr int kSz = 128;
            siv::PerlinNoise pn(m_WorldSeed + 77u);
            std::vector<uint8_t> pix(kSz * kSz * 4);
            for (int py = 0; py < kSz; ++py) {
                for (int px = 0; px < kSz; ++px) {
                    double u = px / (double)kSz;
                    double v = py / (double)kSz;
                    float n1 = (float)pn.octave2D_01(u * 9.0,        v * 9.0,        5, 0.55);
                    float n2 = (float)pn.octave2D_01(u * 26.0 + 5.3, v * 26.0 + 5.3, 2, 0.50);
                    float b  = 0.68f + n1 * 0.50f + n2 * 0.06f;
                    if (b > 1.f) b = 1.f;
                    float r  = b * 0.90f; if (r > 1.f) r = 1.f;
                    float g  = b * 1.05f; if (g > 1.f) g = 1.f;
                    float bl = b * 0.80f;
                    int idx  = (py * kSz + px) * 4;
                    pix[idx+0] = (uint8_t)(r  * 255);
                    pix[idx+1] = (uint8_t)(g  * 255);
                    pix[idx+2] = (uint8_t)(bl * 255);
                    pix[idx+3] = 255;
                }
            }
            return AssetManager::LoadTexture("terrain_detail_noise",
                                             pix.data(), kSz, kSz);
        }();

        Material terrainMat{"Terrain"};
        terrainMat.baseColorTexture = terrainDetailTex;

        // --- Per-chunk terrain meshes for rendering with fog/frustum culling ---
        constexpr float kTerrainUVScale = 10.0f;
        constexpr float kWarpFreq  = 0.011f;
        constexpr float kWarpAmp   = 0.65f;
        siv::PerlinNoise uvWarpNoise(m_WorldSeed + 31u);
        TerrainMeshBuilder::ChunkBuildResult chunkRes = TerrainMeshBuilder::BuildChunks(m_World);
        m_ChunkSize     = chunkRes.chunkSize;
        m_ChunksPerAxis = chunkRes.chunksPerAxis;

        spdlog::info("GameScene: generating {} terrain chunks...", chunkRes.chunks.size());

        // Use a single command buffer for all chunk uploads to avoid GPU
        // memory exhaustion on low‑VRAM devices (i5‑9400 UHD 630).
        SDL_GPUCommandBuffer* uploadCmd = SDL_AcquireGPUCommandBuffer(ctx.renderer->GetDevice());

        for (int cz = 0; cz < chunkRes.chunksPerAxis; ++cz) {
            for (int cx = 0; cx < chunkRes.chunksPerAxis; ++cx) {
                int idx = cz * chunkRes.chunksPerAxis + cx;
                TerrainMeshBuilder::TerrainMeshData& chunk = chunkRes.chunks[idx];

                // World-space UV projection with domain warp
                for (auto& v : chunk.vertices) {
                    if (v.normal.y > 0.5f) {
                        float wx = (float)uvWarpNoise.noise2D(
                            v.position.x * kWarpFreq, v.position.z * kWarpFreq);
                        float wz = (float)uvWarpNoise.noise2D(
                            v.position.x * kWarpFreq + 47.3, v.position.z * kWarpFreq + 47.3);
                        v.texCoords = {
                            v.position.x / kTerrainUVScale + wx * kWarpAmp,
                            v.position.z / kTerrainUVScale + wz * kWarpAmp
                        };
                    }
                }

                std::string key = "procedural://terrain/chunk_" + std::to_string(cx) + "_" + std::to_string(cz);
                std::vector<MeshSection> sections = {
                    { 0, (uint32_t)chunk.indices.size(), 0 }
                };
                std::vector<Material> materials = { terrainMat };
                AssetManager::RegisterProceduralScene(key, chunk.vertices, chunk.indices,
                                                       sections, materials, uploadCmd);

                auto chunkEntity = ctx.clientRegistry.create();
                ctx.clientRegistry.emplace<TransformComponent>(chunkEntity);
                ctx.clientRegistry.emplace<ModelComponent>(chunkEntity, key);
                ctx.clientRegistry.emplace<NoFogCullComponent>(chunkEntity);
                ctx.clientRegistry.emplace<TerrainChunkComponent>(chunkEntity, cx, cz);
            }
        }

        SDL_SubmitGPUCommandBuffer(uploadCmd);
        SDL_WaitForGPUIdle(ctx.renderer->GetDevice());

        // (The merged-mesh build for Jolt MeshShape collision was removed —
        // it took 25+ seconds on a 750x750 map and the result was only used
        // by the MeshCollisionBuilder call below, which we skip for the same
        // reason. Units sample ground height from WorldManager directly.)

        spdlog::info("GameScene: built {} terrain chunks ({}x{}) total {} tiles",
                      chunkRes.chunks.size(), chunkRes.chunksPerAxis, chunkRes.chunksPerAxis,
                      m_World.GetGridSize());
    }

    if (ctx.network.IsHosting()) {
        // NOTE: building a Jolt MeshShape over the full terrain (~3M vertices on
        // a 750x750 map) takes 25+ seconds and can OOM. Skip it for now; ground
        // height for units is sampled directly from the WorldManager via
        // GroundHeightAt(), so we don't lose movement correctness.
        spdlog::info("GameScene: skipping terrain MeshShape build (too expensive on chunked terrain)");
        (void)terrainMesh;

        // Initialize resource system. The default placeholder spawn points
        // would all cluster around the origin; instead, procedurally scatter
        // ~240 nodes (Holz-heavy) across the full map extent so workers have
        // somewhere to walk to from their respective MainBase.
        m_ResourceManager.GenerateForWorld(m_World.GetConfig().worldExtent, m_WorldSeed);
        m_ResourceManager.SpawnPermanentResources(ctx.serverRegistry);

        // Snap every freshly-spawned resource node onto the terrain surface
        // (the generator left Y=0 since it doesn't know the WorldManager).
        {
            auto rView = ctx.serverRegistry.view<TransformComponent, ResourceComponent>();
            for (auto e : rView) {
                auto& tf = rView.get<TransformComponent>(e);
                tf.position.y = GroundHeightAt(m_World, tf.position.x, tf.position.z);
            }
        }

        // Assign netIds to all permanent resources and broadcast to clients
        SyncResourceSpawns(ctx);

        // Per-team Fog of War grids – initialise to match the map extents
        {
            auto pView = ctx.serverRegistry.view<PlayerComponent>();
            for (auto pe : pView) {
                uint32_t teamId = pView.get<PlayerComponent>(pe).playerId;
                FogGrid fg;
                fg.Init(glm::vec3{-375.f, 0.f, -375.f},
                        glm::vec3{ 375.f, 0.f,  375.f},
                        /*cellSize=*/1.f);
                m_TeamFogs[teamId] = std::move(fg);
            }
        }

        // Territory zones — derived from the tile grid
        TerritorySystem::SpawnZones(ctx.serverRegistry, m_World);

        // HUD-Texturen laden (Pixel-Art-Icons des HUD-Designers)
        HUDTextures::Load(ctx.renderer->GetDevice());

        // -----------------------------------------------------------------
        // Spawn MainBase at a unique territory for each connected player.
        // Each player gets their own teamId (= playerId) for FFA.
        // Skips BossArena territories.
        // -----------------------------------------------------------------
        {
            const auto& terr = m_World.GetTerrains();

            // Build a lookup from bugClass → territory spawn point
            std::unordered_map<BugClass, glm::vec2> bugClassToSpawn;
            for (const auto& td : terr) {
                if (td.bugClass == BugClass::BossArena) continue;
                bugClassToSpawn[td.bugClass] = td.spawnPoint;
            }

            auto pView = ctx.serverRegistry.view<PlayerComponent, NetworkedComponent>();
            std::vector<std::pair<uint32_t, entt::entity>> sortedPlayers;
            for (auto pe : pView) {
                auto& pc = pView.get<PlayerComponent>(pe);
                sortedPlayers.emplace_back(pc.playerId, pe);
            }
            std::sort(sortedPlayers.begin(), sortedPlayers.end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });

            for (auto& [playerId, pe] : sortedPlayers) {
                auto& pc = pView.get<PlayerComponent>(pe);
                auto it = bugClassToSpawn.find(pc.bugClass);
                if (it == bugClassToSpawn.end()) {
                    spdlog::warn("GameScene: no territory for bugClass {}", (int)pc.bugClass);
                    continue;
                }

                const glm::vec2 sp = it->second;
                SpawnBuilding(ctx, BuildingType::Main, playerId,
                              glm::vec3{sp.x, 0.f, sp.y}, 1, "assets/cube.glb");

                spdlog::info("GameScene: spawned MainBase for player {} team {} bugClass {} at ({:.1f}, {:.1f})",
                             playerId, playerId, (int)pc.bugClass, sp.x, sp.y);

                // Initial worker grant: 5 free workers per player, scattered in a
                // small ring around the base so they don't all overlap at spawn.
                constexpr int kInitialWorkers = 5;
                constexpr float kSpawnRingRadius = 4.f;
                for (int w = 0; w < kInitialWorkers; ++w) {
                    float ang = (float)w * (6.2831853f / kInitialWorkers);
                    glm::vec3 wp{sp.x + std::cos(ang) * kSpawnRingRadius,
                                 0.f,
                                 sp.y + std::sin(ang) * kSpawnRingRadius};
                    SpawnWorker(ctx, playerId, wp, pc.bugClass);
                }
            }
        }

        // Initial fog reveal around every Main Base for the owning team
        {
            auto bView = ctx.serverRegistry.view<BuildingComponent, TransformComponent>();
            for (auto e : bView) {
                auto& bc = bView.get<BuildingComponent>(e);
                auto& tf = bView.get<TransformComponent>(e);
                if (bc.type == BuildingType::Main) {
                    auto it = m_TeamFogs.find(bc.teamId);
                    if (it != m_TeamFogs.end())
                        it->second.Reveal(tf.position, 40.f);
                }
            }
        }

        // Position camera above local player's Main Base
        {
            auto bView = ctx.serverRegistry.view<BuildingComponent, TransformComponent>();
            for (auto e : bView) {
                auto& bc = bView.get<BuildingComponent>(e);
                auto& tf = bView.get<TransformComponent>(e);
                if (bc.type == BuildingType::Main && bc.teamId == m_MyPlayerId) {
                    m_Camera->m_Position = glm::vec3(tf.position.x, m_TopDownHeight, tf.position.z);
                    m_Camera->UpdateVectors();
                    break;
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // Decorative prop scatter (grass, rocks, twigs, …)
    //
    // Runs on EVERY peer, not just the host: scatter props are derived
    // deterministically from the world seed, so each client generates an
    // identical field locally and nothing has to be sent over the network.
    // They live in the client registry alongside other renderable entities.
    // ------------------------------------------------------------------
    {
        ScatterConfig scatterCfg = ScatterConfig::Default(m_WorldSeed);
        // Align the scatter footprint with the Fog/Territory map extents (±250).
        scatterCfg.worldMin = {-375.f, -375.f};
        scatterCfg.worldMax = { 375.f,  375.f};
        // Props sit on the terraced terrain mesh: TierToWorldHeight() already
        // returns world-space Y, so no extra scaling is needed.
        scatterCfg.heightWorldScale = 1.0f;
        // GPU instancing batches all props of the same model into a single
        // draw call, so we can afford a dense candidate grid.
        scatterCfg.spacing = 7.0f;

        ScatterSystem::Populate(ctx.clientRegistry, m_World, scatterCfg);
    }

    // Dense grass pass — separate grid so grass doesn't compete with trees.
    // Spacing=5 gives ~100x100=10k candidates; maxProps caps draw calls until
    // instancing is added.
    {
        ScatterConfig grassCfg = ScatterConfig::Default(m_WorldSeed + 99u);
        grassCfg.worldMin        = {-375.f, -375.f};
        grassCfg.worldMax        = { 375.f,  375.f};
        grassCfg.heightWorldScale = 1.0f;
        grassCfg.spacing         = 8.0f;
        grassCfg.maxProps        = 1500;
        // Keep only the grass layers from the default config.
        std::vector<ScatterLayer> grassOnly;
        for (auto& l : grassCfg.layers)
            if (l.name.rfind("grass_", 0) == 0)
                grassOnly.push_back(l);
        grassCfg.layers = std::move(grassOnly);

        ScatterSystem::Populate(ctx.clientRegistry, m_World, grassCfg);
    }

    // Build GPU instance buffers for all scatter entities. Done once here;
    // every frame the render pass binds these SSBOs instead of issuing
    // one draw call per entity.
    // Use fog filter on the host (fog grids are already initialised);
    // clients receive fog later via network and will rebuild then.
    {
        const FogGrid* fogPtr = nullptr;
        if (ctx.network.IsHosting()) {
            const FogGrid& f = LocalFog(ctx);
            if (f.IsInitialised()) fogPtr = &f;
        }
        BuildScatterBatches(ctx, fogPtr);
    }

    // ------------------------------------------------------------------
    // Structure placement (faction bases + neutral resource nodes).
    //
    // Like scatter, runs identically on every peer from the shared seed.
    // Faction bases are placed at each territory's pre-computed spawn tile;
    // neutral nodes fill remaining buildable tiles. Both use placeholder
    // primitive models until real structure assets exist.
    // ------------------------------------------------------------------
    {
        StructurePlacementConfig structCfg;
        structCfg.seed           = m_WorldSeed;
        structCfg.maxNeutralNodes = 16;
        structCfg.nodeMinSpacing  = 15.0f;
        StructurePlacementSystem::Place(ctx.clientRegistry, m_World, structCfg);
    }
}

/**
 * @brief Clears registries and resets network/rendering state on exit.
 * @param ctx The scene context.
 */
void GameScene::OnExit(SceneContext& ctx) {
    if (ctx.network.IsHosting()) {
        auto view = ctx.serverRegistry.view<PhysicsBodyComponent>();
        for (auto entity : view) {
            auto& body = view.get<PhysicsBodyComponent>(entity);
            if (ctx.physics && body.handle.IsValid()) {
                // Retrieve the physics ID from Jolt UserData to unregister from World
                uint32_t physicsId = static_cast<uint32_t>(ctx.physics->GetSystem().GetBodyInterface().GetUserData(body.handle.id));
                ctx.world->UnregisterPhysicsEntity(physicsId);
                ctx.physics->RemoveBody(body.handle);
            }
        }

        // Remove static mesh collision bodies
        if (ctx.physics) {
            for (auto& meshBody : m_MeshCollisionBodies) {
                if (meshBody.IsValid())
                    ctx.physics->RemoveBody(meshBody);
            }
        }
        m_MeshCollisionBodies.clear();
    }

    // Release GPU instance buffers before clearing ECS entities.
    m_ScatterBatches.clear();
    m_IdentityInstanceBuffer.reset();

    // Drop decorative scatter props and structures before clearing the rest.
    ScatterSystem::Clear(ctx.clientRegistry);
    StructurePlacementSystem::Clear(ctx.clientRegistry);

    if (ctx.network.IsConnected()) {
        // Returning to Lobby: keep player entities, clear everything else.
        // Destroy non-player entities from server registry.
        {
            std::vector<entt::entity> toDestroy;
            auto view = ctx.serverRegistry.view<entt::entity>(entt::exclude<PlayerComponent>);
            for (auto e : view) toDestroy.push_back(e);
            for (auto e : toDestroy) ctx.serverRegistry.destroy(e);
        }
        // Destroy non-player entities from client registry.
        {
            std::vector<entt::entity> toDestroy;
            auto view = ctx.clientRegistry.view<entt::entity>(entt::exclude<PlayerComponent>);
            for (auto e : view) toDestroy.push_back(e);
            for (auto e : toDestroy) ctx.clientRegistry.destroy(e);
        }
        // Clear CPU-side maps but keep serverNetMap entries for player entities
        {
            std::unordered_map<uint32_t, entt::entity> playerMap;
            for (auto& [netId, e] : m_ServerNetMap) {
                if (ctx.serverRegistry.valid(e) &&
                    ctx.serverRegistry.any_of<PlayerComponent>(e))
                    playerMap[netId] = e;
            }
            m_ServerNetMap = std::move(playerMap);
        }
        {
            std::unordered_map<uint32_t, entt::entity> playerMap;
            for (auto& [netId, e] : m_ClientNetMap) {
                if (ctx.clientRegistry.valid(e) &&
                    ctx.clientRegistry.any_of<PlayerComponent>(e))
                    playerMap[netId] = e;
            }
            m_ClientNetMap = std::move(playerMap);
        }
    } else {
        ctx.serverRegistry.clear();
        ctx.clientRegistry.clear();
        m_ServerNetMap.clear();
        m_PeerToNetId.clear();
        m_ClientNetMap.clear();
        m_NextNetId     = 1;
        m_NextPlayerId  = 0;
        m_MyPlayerId    = 0;
        m_MyNetId       = 0;
        m_IdAssigned    = false;
    }
    m_SnapAccum     = 0.f;
    m_SelectedUnits.clear();
    m_GameOver      = false;
    m_WinnerTeam    = 0xFFFFFFFFu;
    m_CameraMode    = CameraMode::Building;
    if (m_Camera)
        m_Camera->SetProjectionMode(ProjectionMode::Orthographic);

    // Reset all team fog grids for next session
    for (auto& [tid, fg] : m_TeamFogs) fg.Reset();

    // Mark scatter batches for rebuild on next session
    m_ScatterBatchesDirty = true;
    m_FogTextureDirty = true;

    // Release map texture
    m_MapTexture.reset();
    m_MapTextureDirty = true;

    // Reset client-side synced state
    m_ClientFog.Reset();
    m_HasFogData         = false;
    m_ClientTerritories.clear();
    m_TerritorySnapAccum = 0.f;
    m_FogSnapAccum       = 0.f;
    m_InventorySnapAccum = 0.f;
    m_ClientBaseInventories.clear();
    m_ResourceNetMap.clear();

    // HUD-Texturen freigeben
    HUDTextures::Unload();

    ctx.world->ClearPhysicsState();
    
    CancelPlacement(ctx);
    m_VertShader.reset();
    m_FragShader.reset();
    m_ModelPipeline = nullptr;
    m_GhostPipeline = nullptr;

    m_ShadowVertShader.reset();
    m_ShadowFragShader.reset();
    m_ShadowPipeline = nullptr;
    m_ShadowMap.reset();
    m_ShadowUBO.reset();

    // Pipelines outlive their shader objects (SDL_GPU compiles shader code into
    // the pipeline at creation time, so the shader handles can be freed safely).

    spdlog::info("GameScene: exited, registries cleared");
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

/**
 * @brief Handles 3D rendering of the game scene, including models and lights.
 * @param ctx The scene context.
 * @param renderer Pointer to the renderer.
 */
// Push constant struct shared by model and scatter pipelines.
// Must match `layout(set = 3, binding = 0)` in model.frag.
struct alignas(16) FragPC {
    uint32_t matIdx;
    uint32_t objID;
    float alpha;
    float blocksView;
    float fogEnabled;
    float _pad0, _pad1, _pad2;
    glm::vec4 ghostPos;
    glm::vec4 tintColor;
};
static_assert(sizeof(FragPC) == 64, "FragPC must be 64 bytes for std140 layout");

void GameScene::EnsureFogTexture(const SceneContext& ctx) {
    if (m_FogTexture || !ctx.renderer) return;
    // 1x1 fully-revealed placeholder so the model.frag sampler at binding 2
    // is never bound to a null descriptor. Without this the client crashes
    // hard on first frame (Windows TDR / no Vulkan validation output) because
    // the fog code only runs after a FogSnapshotPacket arrives — but the
    // shader pipeline references this slot from the very first draw call.
    uint8_t white[4] = {255, 255, 255, 255};
    m_FogTexture = std::make_shared<Texture>(ctx.renderer->GetDevice(), white,
                                              1, 1, TextureFilter::Nearest);
}

void GameScene::UpdateFogTexture(const SceneContext& ctx, const FogGrid& fog) {
    if (!ctx.renderer) return;
    int w = fog.cellsX;
    int h = fog.cellsZ;
    if (w <= 0 || h <= 0) return;
    std::vector<uint8_t> pixels(w * h * 4, 0);
    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            bool revealed = fog.IsRevealed(x, z);
            int idx = (z * w + x) * 4;
            uint8_t v = revealed ? 255 : 0;
            pixels[idx + 0] = v;
            pixels[idx + 1] = v;
            pixels[idx + 2] = v;
            pixels[idx + 3] = 255;
        }
    }
    m_FogTexture = std::make_shared<Texture>(ctx.renderer->GetDevice(), pixels.data(),
                                              (uint32_t)w, (uint32_t)h,
                                              TextureFilter::Nearest);
}

void GameScene::Render(SceneContext& ctx, Renderer* renderer) {
    if (!m_Camera) return;
    // Always make sure the fog-of-war texture slot is bound. On the client
    // it would otherwise stay null until the first FogSnapshotPacket arrives,
    // and SDL_GPU on Vulkan hard-crashes (TDR) on a draw with an unbound
    // sampled texture descriptor.
    EnsureFogTexture(ctx);
    // ------------------------------------------------------------------
    // 1. Lazy-init graphics pipelines
    // ------------------------------------------------------------------
    if (!m_ModelPipeline) {
        ShaderResourceLayout vertLayout = {0, 0, 2, 1}; // 2 SSBOs: GlobalUniforms + InstanceTransforms
        ShaderResourceLayout fragLayout = {3, 0, 2, 1}; // 3 samplers: baseColor(0), shadowMap(1), fogTexture(2); 2 storage buffers at 3,4

        m_VertShader = std::make_unique<Shader>(renderer->GetDevice(), "shaders/model.vert", ShaderStage::Vertex, vertLayout);
        m_FragShader = std::make_unique<Shader>(renderer->GetDevice(), "shaders/model.frag", ShaderStage::Fragment, fragLayout);

        PipelineConfig config;
        config.vertexShader = m_VertShader.get();
        config.fragmentShader = m_FragShader.get();
        config.vertexStride = sizeof(ModelVertex);
        config.vertexAttributes = {
            {0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(ModelVertex, position)},
            {1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(ModelVertex, normal)},
            {2, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(ModelVertex, texCoords)},
            {3, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(ModelVertex, color)},
            {4, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(ModelVertex, tangent)}
        };
        config.enableDepthTest = true;
        config.depthCompareOp = SDL_GPU_COMPAREOP_GREATER;

        // Match G-Buffer formats from PostProcessor
        config.colorTargetFormats = {
            SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, // Normal
            SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,     // Color (Albedo)
            SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, // Light Buffer
            SDL_GPU_TEXTUREFORMAT_R16_UINT            // Object ID
        };

        m_ModelPipeline = renderer->GetPipelines()->CreatePipeline("GameModelPipeline", config, SDL_GPU_TEXTUREFORMAT_INVALID);
    }

    if (!m_GhostPipeline) {
        // Reuse the same shaders as the model pipeline but with blending enabled
        PipelineConfig ghostCfg;
        ghostCfg.vertexShader = m_VertShader.get();
        ghostCfg.fragmentShader = m_FragShader.get();
        ghostCfg.vertexStride = sizeof(ModelVertex);
        ghostCfg.vertexAttributes = {
            {0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(ModelVertex, position)},
            {1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(ModelVertex, normal)},
            {2, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(ModelVertex, texCoords)},
            {3, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(ModelVertex, color)},
            {4, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(ModelVertex, tangent)}
        };
        ghostCfg.enableDepthTest = true;
        ghostCfg.depthCompareOp = SDL_GPU_COMPAREOP_GREATER;
        ghostCfg.enableBlending = true;
        ghostCfg.colorTargetFormats = {
            SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
            SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
            SDL_GPU_TEXTUREFORMAT_R16_UINT
        };
        m_GhostPipeline = renderer->GetPipelines()->CreatePipeline("GameGhostPipeline", ghostCfg, SDL_GPU_TEXTUREFORMAT_INVALID);
    }

    if (!m_ShadowPipeline ||
        m_ShadowBiasConstant != m_LastShadowBiasConstant ||
        m_ShadowBiasSlope    != m_LastShadowBiasSlope) {
        
        m_ShadowPipeline = nullptr;
        m_LastShadowBiasConstant = m_ShadowBiasConstant;
        m_LastShadowBiasSlope    = m_ShadowBiasSlope;

        ShaderResourceLayout shadowVertLayout = {0, 0, 1, 2}; // 1 SSBO: InstanceTransforms; 2 uniforms: ShadowPC + ShadowVP
        ShaderResourceLayout shadowFragLayout = {0, 0, 0, 0};

        m_ShadowVertShader = std::make_unique<Shader>(renderer->GetDevice(), "shaders/shadow.vert", ShaderStage::Vertex, shadowVertLayout);
        m_ShadowFragShader = std::make_unique<Shader>(renderer->GetDevice(), "shaders/shadow.frag", ShaderStage::Fragment, shadowFragLayout);

        PipelineConfig shadowCfg;
        shadowCfg.vertexShader = m_ShadowVertShader.get();
        shadowCfg.fragmentShader = m_ShadowFragShader.get();
        shadowCfg.vertexStride = sizeof(ModelVertex);
        shadowCfg.vertexAttributes = {
            {0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(ModelVertex, position)}
        };
        shadowCfg.enableDepthTest = true;
        shadowCfg.depthCompareOp = SDL_GPU_COMPAREOP_LESS;
        shadowCfg.cullMode = SDL_GPU_CULLMODE_FRONT;

        shadowCfg.enableDepthBias = true;
        shadowCfg.depthBiasConstantFactor = m_ShadowBiasConstant;
        shadowCfg.depthBiasSlopeFactor = m_ShadowBiasSlope;
        shadowCfg.depthBiasClamp = 0.0f;
        shadowCfg.colorTargetFormats = {};

        m_ShadowPipeline = renderer->GetPipelines()->CreatePipeline("ShadowPipeline", shadowCfg, SDL_GPU_TEXTUREFORMAT_INVALID);
    }

    // ------------------------------------------------------------------
    // 2. Camera matrices
    // ------------------------------------------------------------------
    int w, h;
    SDL_GetWindowSizeInPixels(renderer->GetWindow()->handle, &w, &h);
    float aspect = (float)w / (float)h;

    GlobalUniforms& globals = renderer->GetGlobalUniforms();
    globals.view = m_Camera->GetViewMatrix();
    globals.proj = m_Camera->GetProjectionMatrix(aspect);
    globals.viewProj = globals.proj * globals.view;
    globals.cameraPos = glm::vec4(m_Camera->m_Position, 1.0f);
    
    // ------------------------------------------------------------------
    // 3. Sun light + sunVP matrix for shadow mapping
    // ------------------------------------------------------------------
    globals.sunDir = glm::vec4(glm::normalize(-m_SunDirection), 0.0f);
    globals.sunColor = glm::vec4(m_SunColor, m_SunIntensity);

    {
        const float shadowOrthoSize = m_ShadowOrthoSize;
        const float shadowNear = 0.1f;
        const float shadowFar = 200.0f;

        glm::vec3 sunDir = glm::normalize(m_SunDirection);
        glm::vec3 up = (glm::abs(sunDir.y) < 0.99f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);

        glm::vec3 center = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 sunPos = center - sunDir * 100.0f;
        glm::vec3 target = center + sunDir;

        glm::mat4 sunView = glm::lookAt(sunPos, target, up);
        glm::mat4 sunProj = glm::ortho(-shadowOrthoSize, shadowOrthoSize, -shadowOrthoSize, shadowOrthoSize, shadowNear, shadowFar);

        // Texel Snapping
        float texelSize = (2.0f * shadowOrthoSize) / 2048.0f;
        glm::vec4 originLV = sunView * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        glm::vec2 snappedXY = glm::round(glm::vec2(originLV) / texelSize) * texelSize;
        glm::vec2 snapDelta = snappedXY - glm::vec2(originLV);

        glm::vec3 lightRight = glm::vec3(sunView[0][0], sunView[1][0], sunView[2][0]);
        glm::vec3 lightUp    = glm::vec3(sunView[0][1], sunView[1][1], sunView[2][1]);
        sunPos += lightRight * snapDelta.x + lightUp * snapDelta.y;
        sunView = glm::lookAt(sunPos, sunPos + sunDir, up);

        globals.sunVP = sunProj * sunView;
    }

    // ------------------------------------------------------------------
    // 4. Ambient light
    // ------------------------------------------------------------------
    globals.ambientColor = glm::vec4(m_AmbientColor, m_AmbientIntensity);

    // ------------------------------------------------------------------
    // 5. Dynamic lights
    // ------------------------------------------------------------------
    uint32_t totalLightCount = 0;

    // 5a. Lights from ECS entities
    {
        auto lightView = ctx.clientRegistry.view<TransformComponent, LightComponent>();
        for (auto entity : lightView) {
            if (totalLightCount >= 16) break;
            auto& tf = lightView.get<TransformComponent>(entity);
            auto& lc = lightView.get<LightComponent>(entity);

            Light& out = globals.lights[totalLightCount];
            out.position_type = glm::vec4(tf.position, (float)lc.type);
            out.direction_range = glm::vec4(glm::normalize(lc.direction), lc.range);
            out.color_intensity = glm::vec4(lc.color, lc.intensity);
            totalLightCount++;
        }
    }

    // 5b. Lights embedded inside models (scatter props never have lights — skip them)
    {
        auto modelView = ctx.clientRegistry.view<TransformComponent, ModelComponent>(entt::exclude<ScatterPropComponent>);
        for (auto entity : modelView) {
            if (totalLightCount >= 16) break;
            auto& transform = modelView.get<TransformComponent>(entity);
            auto& modelComp = modelView.get<ModelComponent>(entity);
            const auto& sceneData = AssetManager::LoadGLTF(modelComp.modelPath);
            if (!sceneData.model) continue;

            glm::mat4 entityMat = glm::translate(glm::mat4(1.0f), transform.position) *
                                  glm::mat4_cast(glm::quat(glm::radians(transform.rotation))) *
                                  glm::scale(glm::mat4(1.0f), transform.scale);

            for (const auto& light : sceneData.lights) {
                if (totalLightCount >= 16) break;
                Light& outLight = globals.lights[totalLightCount];
                outLight = light;

                glm::vec4 worldPos = entityMat * glm::vec4(glm::vec3(light.position_type), 1.0f);
                outLight.position_type = glm::vec4(glm::vec3(worldPos), light.position_type.w);

                if ((int)light.position_type.w == (int)LightType::Directional || (int)light.position_type.w == (int)LightType::Spot) {
                    glm::vec4 worldDir = entityMat * glm::vec4(glm::vec3(light.direction_range), 0.0f);
                    outLight.direction_range = glm::vec4(glm::normalize(glm::vec3(worldDir)), light.direction_range.w);
                }
                totalLightCount++;
            }
        }
    }

    globals.timers = glm::vec4(m_TotalTime, (float)totalLightCount, 0.016f, (float)m_FrameCount);
    renderer->UpdateGlobalUniforms(globals);

    // ------------------------------------------------------------------
    // 6. Shadow Pass
    // ------------------------------------------------------------------
    constexpr uint32_t SHADOW_MAP_SIZE = 2048;
    if (!m_ShadowMap) {
        m_ShadowMap = std::make_unique<Framebuffer>(renderer->GetDevice(), SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, std::vector<SDL_GPUTextureFormat>{}, true, TextureFilter::ShadowCompare);
        spdlog::info("GameScene: Shadow map created ({}x{})", SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
    }

    glm::mat4 sunVP = globals.sunVP;
    renderer->AddPass("ShadowPass", m_ShadowMap.get(), [this, ctx, renderer, sunVP](RenderContext& renderCtx) {
        if (!m_ShadowPipeline || !m_IdentityInstanceBuffer) return;
        renderCtx.BindPipeline(m_ShadowPipeline);
        renderCtx.PushVertexConstants(1, &sunVP, sizeof(glm::mat4));

        // Frustum planes for sun shadow (cull chunks outside the shadow map)
        FrustumPlane sunFrustum[6];
        ExtractFrustumPlanes(sunVP, sunFrustum);
        float we = m_World.GetConfig().worldExtent;
        int cpa = m_ChunksPerAxis;

        // --- Non-scatter entities: one draw per mesh instance (unchanged behavior) ---
        auto view = ctx.clientRegistry.view<TransformComponent, ModelComponent>(entt::exclude<ScatterPropComponent>);
        for (auto entity : view) {
            if (ctx.clientRegistry.any_of<GhostComponent>(entity)) continue;
            auto& transform = view.get<TransformComponent>(entity);
            auto& modelComp = view.get<ModelComponent>(entity);
            const auto& sceneData = AssetManager::LoadGLTF(modelComp.modelPath);
            if (!sceneData.model) continue;

            // Fog of war culling: skip entities in unrevealed cells (except special exclusions)
            if (!ctx.clientRegistry.any_of<FogCoverComponent, NoFogCullComponent>(entity)) {
                bool hiddenByFog = false;
                if (m_HasFogData || ctx.network.IsHosting()) {
                    const FogGrid& fog = LocalFog(ctx);
                    if (fog.IsInitialised() && !fog.IsWorldPosRevealed(transform.position))
                        hiddenByFog = true;
                }
                if (hiddenByFog) continue;
            }

            // Per-chunk terrain culling: frustum + fog
            if (auto* tcc = ctx.clientRegistry.try_get<TerrainChunkComponent>(entity)) {
                // Frustum cull against sun VP
                glm::vec3 min(-we + (float)tcc->chunkX * m_ChunkSize, -5.f, -we + (float)tcc->chunkZ * m_ChunkSize);
                glm::vec3 max(-we + (float)(tcc->chunkX + 1) * m_ChunkSize, 20.f, -we + (float)(tcc->chunkZ + 1) * m_ChunkSize);
                if (!IsAABBVisible(min, max, sunFrustum)) continue;
                // Fog cull
                if (m_HasFogData || ctx.network.IsHosting()) {
                    const FogGrid& fog = LocalFog(ctx);
                    if (fog.IsInitialised() && !IsChunkRevealed(fog, tcc->chunkX, tcc->chunkZ))
                        continue;
                }
            }

            renderCtx.BindVertexBuffer(sceneData.model->GetVertexBuffer());
            renderCtx.BindIndexBuffer(sceneData.model->GetIndexBuffer());
            renderCtx.BindVertexStorageBuffer(0, m_IdentityInstanceBuffer.get());

            for (const auto& instance : sceneData.meshInstances) {
                struct ShadowPC { glm::mat4 model; } spc;
                glm::mat4 entityMat = glm::translate(glm::mat4(1.0f), transform.position) *
                                      glm::mat4_cast(glm::quat(glm::radians(transform.rotation))) *
                                      glm::scale(glm::mat4(1.0f), transform.scale);
                spc.model = entityMat * instance.transform;
                renderCtx.PushVertexConstants(0, &spc, sizeof(ShadowPC));

                const auto& allSections = sceneData.model->GetSections();
                for (uint32_t i = 0; i < instance.sectionCount; ++i) {
                    const auto& section = allSections[instance.firstSection + i];
                    renderCtx.DrawIndexed(section.indexCount, 1, section.firstIndex);
                }
            }
        }

        // --- Scatter entities: one draw per model×section, instanceCount = N ---
        {
            struct ShadowPC { glm::mat4 model; } spc;
            spc.model = glm::mat4(1.0f); // instance SSBO carries the full world transforms
            renderCtx.PushVertexConstants(0, &spc, sizeof(ShadowPC));

            for (const auto& batch : m_ScatterBatches) {
                const auto& sceneData = AssetManager::LoadGLTF(batch.modelPath);
                if (!sceneData.model) continue;
                if (batch.meshInstanceIdx >= (uint32_t)sceneData.meshInstances.size()) continue;

                renderCtx.BindVertexBuffer(sceneData.model->GetVertexBuffer());
                renderCtx.BindIndexBuffer(sceneData.model->GetIndexBuffer());
                renderCtx.BindVertexStorageBuffer(0, batch.instanceBuffer.get());

                const auto& meshInst   = sceneData.meshInstances[batch.meshInstanceIdx];
                const auto& allSections = sceneData.model->GetSections();
                for (uint32_t i = 0; i < meshInst.sectionCount; ++i) {
                    const auto& section = allSections[meshInst.firstSection + i];
                    renderCtx.DrawIndexed(section.indexCount, batch.instanceCount, section.firstIndex);
                }
            }
        }
    }, true, nullptr, 1.0f);

    // ------------------------------------------------------------------
    // 7. Render pass
    // ------------------------------------------------------------------
    Framebuffer* gbuffer = renderer->GetGBuffer();
    if (!gbuffer) return;

    // Update the per-tile fog texture if the grid has changed.
    // Must happen OUTSIDE any AddPass lambda — GPU texture upload is illegal during command recording.
    if (m_FogTextureDirty) {
        m_FogTextureDirty = false;
        if (m_HasFogData || ctx.network.IsHosting()) {
            const FogGrid& f = LocalFog(ctx);
            if (f.IsInitialised()) UpdateFogTexture(ctx, f);
        }
    }

    renderer->AddPass("GameRenderPass", gbuffer, [this, ctx, renderer](RenderContext& renderCtx) {
        if (!m_IdentityInstanceBuffer) return;
        renderCtx.BindPipeline(m_ModelPipeline);
        renderCtx.BindVertexStorageBuffer(0, renderer->GetGlobalUBO());
        renderCtx.BindFragmentStorageBuffer(0, renderer->GetGlobalUBO());

        if (m_ShadowMap && m_ShadowMap->GetDepthTarget())
            renderCtx.BindFragmentTexture(1, m_ShadowMap->GetDepthTarget());

        // Bind per-tile fog of war texture (sampled in model.frag when fogEnabled > 0)
        if (m_FogTexture)
            renderCtx.BindFragmentTexture(2, m_FogTexture.get());

        // Frustum planes for main camera (cull chunks outside the viewport)
        FrustumPlane camFrustum[6];
        if (m_Camera) {
            int winW = 1, winH = 1;
            if (ctx.renderer && ctx.renderer->GetWindow())
                SDL_GetWindowSizeInPixels(ctx.renderer->GetWindow()->handle, &winW, &winH);
            float aspect = (winH > 0) ? (float)winW / (float)winH : 1.f;
            glm::mat4 vp = m_Camera->GetProjectionMatrix(aspect) * m_Camera->GetViewMatrix();
            ExtractFrustumPlanes(vp, camFrustum);
        }
        float we = m_World.GetConfig().worldExtent;

        // --- Non-scatter entities: per-entity draw (player, terrain, cubes, …) ---
        auto view = ctx.clientRegistry.view<TransformComponent, ModelComponent>(entt::exclude<ScatterPropComponent>);
        for (auto entity : view) {
            if (ctx.clientRegistry.any_of<GhostComponent>(entity)) continue;
            auto& transform = view.get<TransformComponent>(entity);
            auto& modelComp = view.get<ModelComponent>(entity);
            const auto& sceneData = AssetManager::LoadGLTF(modelComp.modelPath);
            if (!sceneData.model) continue;

            // Fog of war culling: skip entities in unrevealed cells (except special exclusions)
            if (!ctx.clientRegistry.any_of<FogCoverComponent, NoFogCullComponent>(entity)) {
                bool hiddenByFog = false;
                if (m_HasFogData || ctx.network.IsHosting()) {
                    const FogGrid& fog = LocalFog(ctx);
                    if (fog.IsInitialised() && !fog.IsWorldPosRevealed(transform.position))
                        hiddenByFog = true;
                }
                if (hiddenByFog) continue;
            }

            // Per-chunk terrain culling: frustum + fog
            bool isTerrainChunk = false;
            if (auto* tcc = ctx.clientRegistry.try_get<TerrainChunkComponent>(entity)) {
                isTerrainChunk = true;
                // Frustum cull against main camera
                glm::vec3 min(-we + (float)tcc->chunkX * m_ChunkSize, -5.f, -we + (float)tcc->chunkZ * m_ChunkSize);
                glm::vec3 max(-we + (float)(tcc->chunkX + 1) * m_ChunkSize, 20.f, -we + (float)(tcc->chunkZ + 1) * m_ChunkSize);
                if (!IsAABBVisible(min, max, camFrustum)) continue;
                // Fog cull (coarse pre-cull: skip if no tile in this chunk is revealed;
                // per-pixel fog is handled by the GPU shader for partially-revealed chunks)
                if (m_HasFogData || ctx.network.IsHosting()) {
                    const FogGrid& fog = LocalFog(ctx);
                    if (fog.IsInitialised() && !IsChunkRevealed(fog, tcc->chunkX, tcc->chunkZ))
                        continue;
                }
            }

            renderCtx.BindVertexBuffer(sceneData.model->GetVertexBuffer());
            renderCtx.BindIndexBuffer(sceneData.model->GetIndexBuffer());
            renderCtx.BindVertexStorageBuffer(1, m_IdentityInstanceBuffer.get());

            if (sceneData.model->GetMaterialBuffer())
                renderCtx.BindFragmentStorageBuffer(1, sceneData.model->GetMaterialBuffer());

            const auto& allSections  = sceneData.model->GetSections();
            const auto& allMaterials = sceneData.model->GetMaterials();

            for (const auto& instance : sceneData.meshInstances) {
                struct ModelPC { glm::mat4 model; } modelPC;
                glm::mat4 entityMat = glm::translate(glm::mat4(1.0f), transform.position) *
                                      glm::mat4_cast(glm::quat(glm::radians(transform.rotation))) *
                                      glm::scale(glm::mat4(1.0f), transform.scale);
                modelPC.model = entityMat * instance.transform;
                renderCtx.PushVertexConstants(0, &modelPC, sizeof(ModelPC));

                for (uint32_t i = 0; i < instance.sectionCount; ++i) {
                    const auto& section = allSections[instance.firstSection + i];
                    FragPC fpc;
                    fpc.matIdx = (uint32_t)section.materialIndex;
                    fpc.objID  = 0;
                    fpc.alpha  = 1.0f;
                    fpc.blocksView = 0.f;
                    fpc.fogEnabled = isTerrainChunk ? 1.f : 0.f;
                    fpc._pad0 = fpc._pad1 = fpc._pad2 = 0.f;
                    fpc.ghostPos   = glm::vec4(0.f);
                    fpc.tintColor  = glm::vec4(1.f);
                    renderCtx.PushFragmentConstants(0, &fpc, sizeof(FragPC));

                    if (section.materialIndex < allMaterials.size()) {
                        auto tex = allMaterials[section.materialIndex].baseColorTexture;
                        if (!tex) tex = AssetManager::GetFallbackTexture();
                        renderCtx.BindFragmentTexture(0, tex.get());
                    }
                    renderCtx.DrawIndexed(section.indexCount, 1, section.firstIndex);
                }
            }
        }

        // --- Render ghost entity (transparent, with blending) ---
        if (m_GhostEntity != entt::null && ctx.clientRegistry.valid(m_GhostEntity) &&
            m_GhostPipeline) {
            auto* ghostTf  = ctx.clientRegistry.try_get<TransformComponent>(m_GhostEntity);
            auto* ghostMc  = ctx.clientRegistry.try_get<ModelComponent>(m_GhostEntity);
            if (ghostTf && ghostMc) {
                auto gSceneData = AssetManager::LoadGLTF(ghostMc->modelPath);
                if (gSceneData.model) {
                    renderCtx.BindPipeline(m_GhostPipeline);
                    renderCtx.BindVertexStorageBuffer(0, renderer->GetGlobalUBO());
                    renderCtx.BindFragmentStorageBuffer(0, renderer->GetGlobalUBO());

                    if (m_ShadowMap && m_ShadowMap->GetDepthTarget())
                        renderCtx.BindFragmentTexture(1, m_ShadowMap->GetDepthTarget());

                    renderCtx.BindVertexBuffer(gSceneData.model->GetVertexBuffer());
                    renderCtx.BindIndexBuffer(gSceneData.model->GetIndexBuffer());

                    if (gSceneData.model->GetMaterialBuffer())
                        renderCtx.BindFragmentStorageBuffer(1, gSceneData.model->GetMaterialBuffer());

                    const auto& gSections  = gSceneData.model->GetSections();
                    const auto& gMaterials = gSceneData.model->GetMaterials();

                    for (const auto& instance : gSceneData.meshInstances) {
                        struct ModelPC { glm::mat4 model; } gpc;
                        glm::mat4 entityMat = glm::translate(glm::mat4(1.0f), ghostTf->position) *
                                              glm::mat4_cast(glm::quat(glm::radians(ghostTf->rotation))) *
                                              glm::scale(glm::mat4(1.0f), ghostTf->scale);
                        gpc.model = entityMat * instance.transform;
                        renderCtx.PushVertexConstants(0, &gpc, sizeof(ModelPC));

                        for (uint32_t i = 0; i < instance.sectionCount; ++i) {
                            const auto& section = gSections[instance.firstSection + i];
                            FragPC gfpc;
                            gfpc.matIdx = (uint32_t)section.materialIndex;
                            gfpc.objID  = 0;
                            gfpc.alpha  = 0.35f;
                            gfpc.blocksView = 0.f;
                            gfpc.fogEnabled = 0.f;
                            gfpc._pad0 = gfpc._pad1 = gfpc._pad2 = 0.f;
                            gfpc.ghostPos   = glm::vec4(0.f);
                            gfpc.tintColor  = m_PlacementValid ? glm::vec4(1.f) : glm::vec4(1.f, 0.f, 0.f, 1.f);
                            renderCtx.PushFragmentConstants(0, &gfpc, sizeof(FragPC));

                            if (section.materialIndex < gMaterials.size()) {
                                auto tex = gMaterials[section.materialIndex].baseColorTexture;
                                if (!tex) tex = AssetManager::GetFallbackTexture();
                                renderCtx.BindFragmentTexture(0, tex.get());
                            }
                            renderCtx.DrawIndexed(section.indexCount, 1, section.firstIndex);
                        }
                    }
                }
            }
        }

        // --- Scatter entities: one draw per model×section, instanceCount = N ---
        {
            struct ModelPC { glm::mat4 model; } modelPC;
            modelPC.model = glm::mat4(1.0f); // instance SSBO carries the full world transforms
            renderCtx.PushVertexConstants(0, &modelPC, sizeof(ModelPC));

            for (const auto& batch : m_ScatterBatches) {
                const auto& sceneData = AssetManager::LoadGLTF(batch.modelPath);
                if (!sceneData.model) continue;
                if (batch.meshInstanceIdx >= (uint32_t)sceneData.meshInstances.size()) continue;

                // In building mode, tall assets (trees, cacti, etc.) that can
                // occlude the isometric view are faded out near the ghost.
                bool blocksView = (m_CameraMode == CameraMode::Building) &&
                                  IsTreeModelPath(batch.modelPath);

                renderCtx.BindVertexBuffer(sceneData.model->GetVertexBuffer());
                renderCtx.BindIndexBuffer(sceneData.model->GetIndexBuffer());
                renderCtx.BindVertexStorageBuffer(1, batch.instanceBuffer.get());

                if (sceneData.model->GetMaterialBuffer())
                    renderCtx.BindFragmentStorageBuffer(1, sceneData.model->GetMaterialBuffer());

                const auto& meshInst    = sceneData.meshInstances[batch.meshInstanceIdx];
                const auto& allSections  = sceneData.model->GetSections();
                const auto& allMaterials = sceneData.model->GetMaterials();

                for (uint32_t i = 0; i < meshInst.sectionCount; ++i) {
                    const auto& section = allSections[meshInst.firstSection + i];
                    FragPC fpc;
                    fpc.matIdx = (uint32_t)section.materialIndex;
                    fpc.objID  = 0;
                    fpc.alpha  = 1.0f;
                    fpc.blocksView = blocksView ? 1.f : 0.f;
                    fpc.fogEnabled = 0.f;
                    fpc._pad0 = fpc._pad1 = fpc._pad2 = 0.f;
                    fpc.ghostPos   = blocksView ? glm::vec4(m_FadeCenter, 0.f) : glm::vec4(0.f);
                    fpc.tintColor  = glm::vec4(1.f);
                    renderCtx.PushFragmentConstants(0, &fpc, sizeof(FragPC));

                    if (section.materialIndex < allMaterials.size()) {
                        auto tex = allMaterials[section.materialIndex].baseColorTexture;
                        if (!tex) tex = AssetManager::GetFallbackTexture();
                        renderCtx.BindFragmentTexture(0, tex.get());
                    }
                    renderCtx.DrawIndexed(section.indexCount, batch.instanceCount, section.firstIndex);
                }
            }
        }
    }, true);
}

// ---------------------------------------------------------------------------
// Logic update
// ---------------------------------------------------------------------------

void GameScene::LogicUpdate(SceneContext& ctx, float dt) {
    // F12 toggles all developer ImGui panels at once.
    if (Input::IsKeyPressed(SDLK_F12)) DebugUI::Toggle();

    if (!ctx.network.IsConnected()) {
        ctx.scenes.RequestTransition(new MainMenuScene());
        return;
    }

    if (!m_Camera) return;

    // ------------------------------------------------------------------
    // TAB: cycle Commander ↔ Building (same camera, different interactions)
    // ------------------------------------------------------------------
    if (Input::IsKeyPressed(SDLK_TAB)) {
        if (m_CameraMode == CameraMode::Building)
            CancelPlacement(ctx);
        m_CameraMode = (m_CameraMode == CameraMode::Commander)
                       ? CameraMode::Building : CameraMode::Commander;
        spdlog::info("GameScene: switched to {} mode",
                      m_CameraMode == CameraMode::Commander ? "Commander" : "Building");
    }

    if (m_CameraMode == CameraMode::Commander || m_CameraMode == CameraMode::Building) {
        // ------------------------------------------------------------------
        // Isometric orthographic camera – same for both Commander and Building
        // ------------------------------------------------------------------
        int winW, winH;
        SDL_GetWindowSizeInPixels(ctx.renderer->GetWindow()->handle, &winW, &winH);

        constexpr float TOPDOWN_PAN_SPEED   = 25.f;
        constexpr float BUILD_ROTATE_SPEED  = 60.f;

        // Arrow keys snap 90° to the next/previous diagonal isometric view,
        // orbiting around the ground point at the centre of the screen.
        constexpr float YAW_ANIM_DURATION = 0.25f;
        if (Input::IsKeyPressed(SDLK_LEFT) && m_YawAnimT >= 1.f) {
            // Compute the pivot: the ground-level point at the centre of the view.
            float tPivot = -m_Camera->m_Position.y / m_Camera->m_Front.y;
            m_YawPivot   = m_Camera->m_Position + tPivot * m_Camera->m_Front;
            m_CamPosFrom = m_Camera->m_Position;
            m_BuildYawFrom   = m_BuildYaw;
            m_BuildYawTarget = m_BuildYawFrom + 90.f;
            m_YawAnimT = 0.f;
        }
        if (Input::IsKeyPressed(SDLK_RIGHT) && m_YawAnimT >= 1.f) {
            float tPivot = -m_Camera->m_Position.y / m_Camera->m_Front.y;
            m_YawPivot   = m_Camera->m_Position + tPivot * m_Camera->m_Front;
            m_CamPosFrom = m_Camera->m_Position;
            m_BuildYawFrom   = m_BuildYaw;
            m_BuildYawTarget = m_BuildYawFrom - 90.f;
            m_YawAnimT = 0.f;
        }
        if (m_YawAnimT < 1.f) {
            m_YawAnimT = std::min(m_YawAnimT + dt / YAW_ANIM_DURATION, 1.f);
            // Smoothstep ease-in/ease-out: t * t * (3 - 2 * t)
            float t = m_YawAnimT;
            float eased = t * t * (3.f - 2.f * t);
            m_BuildYaw = glm::mix(m_BuildYawFrom, m_BuildYawTarget, eased);

            // Orbit the camera position around the pivot by (eased) of the 90° arc.
            float angle  = glm::radians(glm::mix(0.f, m_BuildYawTarget - m_BuildYawFrom, eased));
            glm::vec3 off = m_CamPosFrom - m_YawPivot;
            float ca = std::cos(angle);
            float sa = std::sin(angle);
            m_Camera->m_Position = m_YawPivot + glm::vec3(
                off.x * ca - off.z * sa,
                off.y,
                off.x * sa + off.z * ca);
        }
        (void)BUILD_ROTATE_SPEED;

        m_Camera->m_Pitch = -55.f;
        m_Camera->m_Yaw   = m_BuildYaw;
        m_Camera->UpdateVectors();

        // WASD pans relative to the isometric camera view
        glm::vec3 forward = glm::normalize(glm::vec3(m_Camera->m_Front.x, 0.f, m_Camera->m_Front.z));
        glm::vec3 right   = m_Camera->m_Right;
        glm::vec3 pan{0.f};
        if (Input::IsKeyDown(SDLK_W)) pan += forward * TOPDOWN_PAN_SPEED * dt;
        if (Input::IsKeyDown(SDLK_S)) pan -= forward * TOPDOWN_PAN_SPEED * dt;
        if (Input::IsKeyDown(SDLK_A)) pan -= right   * TOPDOWN_PAN_SPEED * dt;
        if (Input::IsKeyDown(SDLK_D)) pan += right   * TOPDOWN_PAN_SPEED * dt;
        m_Camera->m_Position += pan;

        // Scroll to zoom (adjust ortho size) — skip if mouse is over ImGui UI
        if (!ImGui::GetIO().WantCaptureMouse) {
            float scroll = Input::GetMouseWheelDelta();
            if (scroll != 0.f) {
                m_BuildOrthoSize = glm::clamp(m_BuildOrthoSize - scroll * 3.f, 5.f, 80.f);
                m_Camera->m_OrthoSize = m_BuildOrthoSize;
                spdlog::debug("Zoom: orthoSize={:.1f}", m_BuildOrthoSize);
            }
        }

        // Building mode: cursor world position, tree fade, placement
        if (m_CameraMode == CameraMode::Building) {
            glm::vec2 mpos  = Input::GetMousePosition();
            glm::vec3 rawCursor = ScreenToWorldXZ(mpos.x, mpos.y, winW, winH);
            m_FadeCenter = rawCursor;

            // Placement ghost follow cursor
            if (m_PlacementActive && m_GhostEntity != entt::null &&
                ctx.clientRegistry.valid(m_GhostEntity)) {
                glm::vec3 worldPos = rawCursor;
                // Snap to the worldgen tile grid and sit on the terrace surface.
                int gtx, gtz;
                m_World.WorldToTile(worldPos.x, worldPos.z, gtx, gtz);
                glm::vec2 tileCenter = m_World.TileToWorld(gtx, gtz);
                worldPos.x = tileCenter.x;
                worldPos.z = tileCenter.y;
                worldPos.y = m_World.TierToWorldHeight(m_World.GetTile(gtx, gtz).tier);
                m_GhostPos = worldPos;

                // --- Placement validation ---
                m_PlacementValid = m_World.GetTile(gtx, gtz).buildable;

                // Check no existing building at this position (both registries)
                if (m_PlacementValid) {
                    auto checkBuildings = [&](auto& reg) {
                        auto bldView = reg.template view<BuildingComponent, TransformComponent>();
                        for (auto be : bldView) {
                            auto& btf = bldView.template get<TransformComponent>(be);
                            auto& bc  = bldView.template get<BuildingComponent>(be);
                            if (bc.destroyed) continue;
                            if (glm::distance(btf.position, worldPos) < 1.5f) return false;
                        }
                        return true;
                    };
                    m_PlacementValid = checkBuildings(ctx.serverRegistry) &&
                                       checkBuildings(ctx.clientRegistry);
                }

                // Check no scatter prop at this position
                if (m_PlacementValid) {
                    auto scatView = ctx.clientRegistry.view<TransformComponent, ScatterPropComponent>();
                    for (auto se : scatView) {
                        auto& stf = scatView.get<TransformComponent>(se);
                        if (glm::distance(stf.position, worldPos) < 1.5f) {
                            m_PlacementValid = false;
                            break;
                        }
                    }
                }

                // Block placement in unrevealed fog cells
                {
                    const FogGrid& pfog = LocalFog(ctx);
                    if (m_PlacementValid && pfog.IsInitialised()) {
                        if (!pfog.IsWorldPosRevealed(worldPos))
                            m_PlacementValid = false;
                    }
                }

                auto* tf = ctx.clientRegistry.try_get<TransformComponent>(m_GhostEntity);
                if (tf) tf->position = worldPos;

                // Left-click to place (skip if hovering over UI; only where valid)
                if (Input::IsMouseButtonPressed(SDL_BUTTON_LEFT) &&
                    !ImGui::GetIO().WantCaptureMouse && ctx.network.IsHosting() &&
                    m_PlacementValid) {
                    // Team 0 for host, TODO: proper team assignment
                    uint32_t placeTeam = 0;
                    if (m_SelectedBuildingType >= 0) {
                        SpawnBuilding(ctx, static_cast<BuildingType>(m_SelectedBuildingType),
                                      placeTeam, worldPos);
                    } else if (m_PlacementActive) {
                        // Special building
                        auto info = GetSpecialBuildingInfo(m_SelectedSpecialBuilding);
                        SpawnBuilding(ctx, BuildingType::Special, placeTeam, worldPos,
                                      1, "assets/cube.glb", m_SelectedSpecialBuilding);
                    }
                }

                // Right-click or Escape to cancel
                if (Input::IsMouseButtonPressed(SDL_BUTTON_RIGHT) ||
                    Input::IsKeyPressed(SDLK_ESCAPE)) {
                    CancelPlacement(ctx);
                }
            }
        }

        // Commander mode: unit selection + orders (not in Building mode)
        if (m_CameraMode == CameraMode::Commander) {
            // Left-click: select units near cursor (pick by proximity to XZ ray)
            if (Input::IsMouseButtonPressed(SDL_BUTTON_LEFT)) {
                glm::vec2 mpos = Input::GetMousePosition();
                glm::vec3 worldPos = ScreenToWorldXZ(mpos.x, mpos.y, winW, winH);
                constexpr float SELECT_RADIUS = 5.f;

                bool shiftHeld = Input::IsKeyDown(SDLK_LSHIFT) || Input::IsKeyDown(SDLK_RSHIFT);
                if (!shiftHeld)
                    m_SelectedUnits.clear();

                auto view = ctx.clientRegistry.view<TransformComponent, NetworkedComponent, UnitComponent>();
                for (auto entity : view) {
                    auto& tf  = view.get<TransformComponent>(entity);
                    auto& nc  = view.get<NetworkedComponent>(entity);
                    auto& uc  = view.get<UnitComponent>(entity);
                    if (uc.teamId != m_MyPlayerId) continue;
                    glm::vec2 d2 = glm::vec2(tf.position.x - worldPos.x, tf.position.z - worldPos.z);
                    if (glm::length(d2) <= SELECT_RADIUS) {
                        if (shiftHeld) {
                            // Toggle the clicked unit in/out of the selection.
                            auto it = std::find(m_SelectedUnits.begin(), m_SelectedUnits.end(), nc.netId);
                            if (it != m_SelectedUnits.end()) {
                                m_SelectedUnits.erase(it);
                                uc.selected = false;
                            } else {
                                m_SelectedUnits.push_back(nc.netId);
                                uc.selected = true;
                            }
                        } else {
                            m_SelectedUnits.push_back(nc.netId);
                            uc.selected = true;
                        }
                    } else if (!shiftHeld) {
                        uc.selected = false;
                    }
                }
                spdlog::info("Commander: selected {} unit(s)", m_SelectedUnits.size());
            }

            // Right-click: issue move or attack order to selected units
            if (Input::IsMouseButtonPressed(SDL_BUTTON_RIGHT) && !m_SelectedUnits.empty()) {
                glm::vec2 mpos     = Input::GetMousePosition();
                glm::vec3 worldPos = ScreenToWorldXZ(mpos.x, mpos.y, winW, winH);

                CommanderOrderPacket pkt;
                pkt.playerId = m_MyPlayerId;
                pkt.x = worldPos.x; pkt.y = worldPos.y; pkt.z = worldPos.z;
                pkt.selectedCount = std::min(static_cast<uint32_t>(m_SelectedUnits.size()), 32u);
                for (uint32_t i = 0; i < pkt.selectedCount; ++i)
                    pkt.selectedNetIds[i] = m_SelectedUnits[i];

                // CTRL+RMB: attack order — pick nearest enemy unit or building
                bool ctrlHeld = Input::IsKeyDown(SDLK_LCTRL) || Input::IsKeyDown(SDLK_RCTRL);
                if (ctrlHeld) {
                    constexpr float PICK_RADIUS = 3.f;
                    float bestDist = PICK_RADIUS;
                    uint32_t targetNetId = 0;
                    uint8_t targetType = 0; // 1=unit, 2=building

                    // Check enemy units
                    auto unitView = ctx.clientRegistry.view<TransformComponent, NetworkedComponent, UnitComponent>();
                    for (auto e : unitView) {
                        auto& tf = unitView.get<TransformComponent>(e);
                        auto& nc = unitView.get<NetworkedComponent>(e);
                        auto& uc = unitView.get<UnitComponent>(e);
                        if (uc.teamId == m_MyPlayerId) continue;
                        float d = glm::length(glm::vec2(tf.position.x - worldPos.x, tf.position.z - worldPos.z));
                        if (d < bestDist) {
                            bestDist = d; targetNetId = nc.netId; targetType = 1;
                            pkt.x = tf.position.x; pkt.z = tf.position.z;
                        }
                    }

                    // Check enemy buildings (lower priority than units)
                    auto bldView = ctx.clientRegistry.view<TransformComponent, NetworkedComponent, BuildingComponent>();
                    for (auto e : bldView) {
                        auto& tf = bldView.get<TransformComponent>(e);
                        auto& nc = bldView.get<NetworkedComponent>(e);
                        auto& bc = bldView.get<BuildingComponent>(e);
                        if (bc.teamId == m_MyPlayerId || bc.destroyed) continue;
                        float d = glm::length(glm::vec2(tf.position.x - worldPos.x, tf.position.z - worldPos.z));
                        if (d < bestDist) {
                            bestDist = d; targetNetId = nc.netId; targetType = 2;
                            pkt.x = tf.position.x; pkt.z = tf.position.z;
                        }
                    }

                    if (targetNetId != 0) {
                        pkt.targetNetId = targetNetId;
                        pkt.orderType   = targetType;
                    }
                }

                ctx.network.Send(pkt);
                spdlog::info("Commander: orderType={} to ({:.1f},{:.1f},{:.1f}) for {} units (targetNetId={})",
                             pkt.orderType, pkt.x, pkt.y, pkt.z, pkt.selectedCount, pkt.targetNetId);
            }
        }
    }

    // Construction animation: slide buildings up from below ground
    {
        auto view = ctx.clientRegistry.view<TransformComponent, ConstructionComponent>();
        for (auto e : view) {
            auto& tf = view.get<TransformComponent>(e);
            auto& cc = view.get<ConstructionComponent>(e);
            cc.elapsed += dt;
            float t = std::min(cc.elapsed / cc.duration, 1.f);
            tf.position.y = glm::mix(cc.startY, cc.targetY, t);
            if (cc.elapsed >= cc.duration) {
                tf.position.y = cc.targetY;
                ctx.clientRegistry.remove<ConstructionComponent>(e);
            }
        }
    }

    m_Camera->Update(dt);

    // Per-biome fog: sample camera tile, pick fog colour for that biome.
    if (ctx.postProcessor && m_World.GetGridSize() > 0) {
        int tx, tz;
        m_World.WorldToTile(m_Camera->m_Position.x, m_Camera->m_Position.z, tx, tz);
        const TerrainTile& tile = m_World.GetTile(tx, tz);
        BugClass biome = (tile.territoryId < m_World.GetTerrains().size())
            ? m_World.GetTerrains()[tile.territoryId].bugClass
            : BugClass::None;

        glm::vec3 fogCol;
        float fogDensity = 0.003f;
        switch (biome) {
            case BugClass::Ants:             fogCol = {0.85f, 0.78f, 0.55f}; fogDensity = 0.002f; break; // sandy haze
            case BugClass::Termites:         fogCol = {0.52f, 0.42f, 0.28f}; fogDensity = 0.003f; break;
            case BugClass::Spiders:          fogCol = {0.18f, 0.14f, 0.22f}; fogDensity = 0.005f; break; // dark moody
            case BugClass::Woodlice:         fogCol = {0.58f, 0.60f, 0.65f}; fogDensity = 0.003f; break; // grey mist
            case BugClass::BeesWasps:        fogCol = {0.80f, 0.78f, 0.50f}; fogDensity = 0.002f; break; // golden
            case BugClass::ButterfliesMoths: fogCol = {0.70f, 0.62f, 0.80f}; fogDensity = 0.003f; break; // lavender
            case BugClass::Snails:           fogCol = {0.30f, 0.52f, 0.40f}; fogDensity = 0.004f; break; // wet moss
            case BugClass::Mantis:           fogCol = {0.15f, 0.30f, 0.18f}; fogDensity = 0.005f; break; // deep forest
            case BugClass::Fireflies:        fogCol = {0.10f, 0.18f, 0.32f}; fogDensity = 0.006f; break; // twilight
            case BugClass::CentipedesWorms:  fogCol = {0.20f, 0.16f, 0.12f}; fogDensity = 0.006f; break; // underground
            case BugClass::MosquitosTicks:   fogCol = {0.28f, 0.42f, 0.22f}; fogDensity = 0.005f; break; // swamp
            case BugClass::Dragonflies:      fogCol = {0.18f, 0.42f, 0.52f}; fogDensity = 0.004f; break; // teal pond
            case BugClass::Bugs:             fogCol = {0.50f, 0.62f, 0.15f}; fogDensity = 0.004f; break; // toxic
            case BugClass::Roaches:          fogCol = {0.32f, 0.26f, 0.20f}; fogDensity = 0.005f; break; // decay
            case BugClass::Beetles:          fogCol = {0.22f, 0.20f, 0.25f}; fogDensity = 0.005f; break; // charcoal
            case BugClass::Scorpions:        fogCol = {0.88f, 0.80f, 0.55f}; fogDensity = 0.002f; break; // desert
            default:                         fogCol = {0.65f, 0.72f, 0.80f}; fogDensity = 0.003f; break;
        }
        ctx.postProcessor->SetFog(fogCol, fogDensity, 30.0f);
    }

    m_TotalTime += dt;
    m_FrameCount++;
}

void GameScene::UIUpdate(SceneContext& ctx, float dt) {
    // ------------------------------------------------------------------
    // Game-over overlay
    // ------------------------------------------------------------------
    if (m_GameOver) {
        m_GameOverTimer += dt;

        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 180));
        ImGui::Begin("Spielende", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        if (m_WinnerTeam == m_MyPlayerId)
            ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "SIEG! Team %u gewinnt!", m_WinnerTeam);
        else if (m_WinnerTeam == 0xFFFFFFFFu)
            ImGui::Text("Unentschieden!");
        else
            ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "NIEDERLAGE! Team %u gewinnt.", m_WinnerTeam);

        float remaining = std::max(0.f, GAME_OVER_DELAY - m_GameOverTimer);
        ImGui::TextDisabled("Return to lobby in %.0f seconds...", remaining);

        if (ctx.network.IsHosting()) {
            if (ImGui::Button("Play Again (Lobby)")) {
                ctx.network.BroadcastToAll(ReturnToLobbyPacket{});
                ctx.scenes.RequestTransition(new LobbyScene());
            }
            ImGui::SameLine();
            if (ImGui::Button("Main Menu")) {
                ctx.network.Disconnect();
                ctx.scenes.RequestTransition(new MainMenuScene());
            }

            // Auto-return after delay
            if (m_GameOverTimer >= GAME_OVER_DELAY) {
                ctx.network.BroadcastToAll(ReturnToLobbyPacket{});
                ctx.scenes.RequestTransition(new LobbyScene());
            }
        } else {
            // Clients also auto-transition when host sends ReturnToLobbyPacket
            ImGui::TextDisabled("Waiting for host...");
        }
        ImGui::End();
        return;
    }

    // -- Game HUD: pinned to the bottom-left of the screen, no title bar,
    // no drag, fixed width. Reads like an RTS command bar instead of a
    // floating dev panel. --
    {
        ImGuiIO& io = ImGui::GetIO();
        const float W = 380.f;
        const float marginX = 12.f;
        const float marginY = 12.f;
        ImGui::SetNextWindowPos(ImVec2(marginX, io.DisplaySize.y - marginY),
                                ImGuiCond_Always, ImVec2(0.f, 1.f));
        ImGui::SetNextWindowSizeConstraints(ImVec2(W, 0.f),
                                            ImVec2(W, io.DisplaySize.y * 0.85f));
    }
    constexpr ImGuiWindowFlags kHUDFlags =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::Begin("Game", nullptr, kHUDFlags);

    // Mode pill — compact, looks like a status badge.
    const char* modeLabel = "Unknown";
    switch (m_CameraMode) {
        case CameraMode::Commander: modeLabel = "Commander"; break;
        case CameraMode::Building:  modeLabel = "Building";  break;
    }
    ImGui::TextColored(ImVec4(0.85f, 0.70f, 0.30f, 1.f), "%s", modeLabel);
    ImGui::SameLine();
    ImGui::TextDisabled("[TAB] wechseln");

    // Dev-only identity lines move behind the F12 flag — cleaner default HUD.
    if (DebugUI::IsVisible()) {
        ImGui::Separator();
        ImGui::TextDisabled("%s | playerId=%u netId=%u",
                            ctx.network.IsHosting() ? "Host" : "Client",
                            m_MyPlayerId, m_MyNetId);
        if (!m_IdAssigned) ImGui::TextDisabled("Waiting for server assignment...");

        if (ctx.network.IsHosting()) {
            ImGui::Separator();
            if (ImGui::Button("Eradicate all enemy Main buildings")) {
                auto bView = ctx.serverRegistry.view<BuildingComponent, NetworkedComponent>();
                for (auto be : bView) {
                    auto& bc = bView.get<BuildingComponent>(be);
                    auto& nc = bView.get<NetworkedComponent>(be);
                    if (bc.type == BuildingType::Main && !bc.destroyed && bc.teamId != m_MyPlayerId) {
                        bc.destroyed = true;
                        HandleBuildingDeath(ctx, be, nc.netId);
                    }
                }
            }
        }
    }

    if (m_CameraMode == CameraMode::Commander) {
        ImGui::Separator();
        ImGui::Text("Ausgewaehlte Einheiten: %zu", m_SelectedUnits.size());
        ImGui::TextDisabled("LKlick: Einheit waehlen  RKlick: Bewegungsbefehl");
    }

    if (m_CameraMode == CameraMode::Building) {
        ImGui::Separator();
        ImGui::Text("Ortho-Zoom: %.1f (Mausrad)", m_BuildOrthoSize);
        ImGui::TextDisabled("WASD: Bewegen | Mausrad: Zoomen | G: Grundrisse | S: Spezial");

        // Determine player's tribe for special buildings
        BugClass playerTribe = BugClass::Termites;
        {
            auto pView = ctx.clientRegistry.view<PlayerComponent>();
            for (auto pe : pView) {
                auto& pc = pView.get<PlayerComponent>(pe);
                if (pc.isLocal) {
                    if (pc.bugClass != BugClass::None)
                        playerTribe = pc.bugClass;
                    break;
                }
            }
        }

        // --- Placeable buildings (military/defense only) ---
        struct BldBtn {
            const char* name;
            BuildingType type;
        };
        const BldBtn placeableBlds[] = {
            {"Brutkammer", BuildingType::Barracks},
            {"Speicher",   BuildingType::Storage},
            {"Verteid.",   BuildingType::Defense},
            {"Angriff",    BuildingType::Attack},
            {"Vorposten",  BuildingType::Outpost},
            {"Konversion", BuildingType::Conversion},
        };

        ImGui::Text("Gebaeude platzieren:");
        for (int i = 0; i < (int)(sizeof(placeableBlds)/sizeof(placeableBlds[0])); i++) {
            bool active = (m_PlacementActive &&
                           m_SelectedBuildingType == static_cast<int>(placeableBlds[i].type));
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.f));
            if (ImGui::Button(placeableBlds[i].name)) {
                if (active) CancelPlacement(ctx);
                else        EnterPlacementMode(ctx, placeableBlds[i].type);
            }
            if (active) ImGui::PopStyleColor();
            if ((i + 1) % 3 != 0) ImGui::SameLine();
        }

        // --- Special buildings (tribe-specific) ---
        ImGui::Separator();
        ImGui::Text("Spezialgebaeude (%s):", BugClassName(playerTribe));
        auto specials = GetSpecialBuildingsForTribe(playerTribe);
        for (size_t i = 0; i < specials.size(); i++) {
            auto info = GetSpecialBuildingInfo(specials[i]);
            // Use a negative offset to distinguish special buildings from generic ones
            int specialIdx = -(static_cast<int>(specials[i]) + 1);
            bool active = (m_PlacementActive && m_SelectedBuildingType == specialIdx);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.6f, 1.f));
            if (ImGui::Button(info.name)) {
                if (active) CancelPlacement(ctx);
                else {
                    CancelPlacement(ctx);
                    m_SelectedBuildingType = specialIdx;
                    m_PlacementActive = true;
                    m_SelectedSpecialBuilding = specials[i];
                    // Create ghost
                    m_GhostEntity = ctx.clientRegistry.create();
                    ctx.clientRegistry.emplace<TransformComponent>(m_GhostEntity, glm::vec3{0.f, 0.f, 0.f});
                    ctx.clientRegistry.emplace<ModelComponent>(m_GhostEntity, std::string("assets/cube.glb"));
                    ctx.clientRegistry.emplace<GhostComponent>(m_GhostEntity);
                    spdlog::info("GameScene: entered placement for special building {}", info.name);
                }
            }
            if (active) ImGui::PopStyleColor();
            if (i == 0) ImGui::SameLine();
            if (active && info.description) {
                ImGui::TextDisabled("%s", info.description);
            }
        }

        if (m_PlacementActive) {
            ImGui::TextDisabled("LKlick: platzieren  RKlick/ESC: abbrechen");
        }

        // --- Upgrade section ---
        if (ctx.network.IsHosting()) {
            uint32_t myTeam = m_MyPlayerId;
            auto& ust = UpgradeSystem::GetState(myTeam);
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.3f, 1.f, 0.3f, 1.f), "Upgrades (Basis Level %d)", ust.baseLevel);

            // Available base upgrades
            auto availBase = UpgradeSystem::AvailableBaseUpgrades(ctx.serverRegistry, myTeam);
            for (auto pid : availBase) {
                auto* def = FindUpgradeDef(pid);
                if (!def) continue;
                std::string label = def->name;
                label += "##base";
                if (ImGui::Button(label.c_str())) {
                    ApplyUpgrade(ctx, myTeam, pid);
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", def->description);
            }
            if (availBase.empty() && ust.baseLevel < 3) {
                ImGui::TextDisabled("  (Voraussetzungen nicht erfuellt)");
            } else if (ust.baseLevel >= 3) {
                ImGui::TextDisabled("  (Maximales Level erreicht)");
            }

            // Building upgrade buttons (current selection)
            if (m_SelectedBuildingType >= 0) {
                auto bType = static_cast<BuildingType>(m_SelectedBuildingType);
                auto availBld = UpgradeSystem::AvailableBuildingUpgrades(ctx.serverRegistry, myTeam, bType);
                if (!availBld.empty()) {
                    ImGui::Text("Gebaeude-Upgrades (%s):",
                                BuildingTypeName(bType));
                    for (auto pid : availBld) {
                        auto* def = FindUpgradeDef(pid);
                        if (!def) continue;
                        std::string label = def->name;
                        label += "##bld";
                        if (ImGui::Button(label.c_str())) {
                            ApplyUpgrade(ctx, myTeam, pid);
                        }
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("%s", def->description);
                    }
                }
            }

            // ---------------------------------------------------------------
            // Brutkammer (Barracks): queue + ausbilden + instant-spawn debug.
            // Lists every Barracks owned by the local team with its current
            // production state, and gives a quick way to actually get units.
            // ---------------------------------------------------------------
            ImGui::Separator();
            ImGui::Text("Brutkammern:");
            int barracksIdx = 0;
            auto barrView = ctx.serverRegistry.view<TransformComponent,
                                                     BuildingComponent,
                                                     BarracksComponent>();
            bool anyBarracks = false;
            for (auto e : barrView) {
                const auto& bc = barrView.get<BuildingComponent>(e);
                if (bc.teamId != myTeam || bc.destroyed) continue;
                anyBarracks = true;
                auto& barr = barrView.get<BarracksComponent>(e);
                const auto& tf = barrView.get<TransformComponent>(e);

                ImGui::PushID(barracksIdx++);
                ImGui::Text("Brutkammer @ (%.0f, %.0f)  Queue: %zu/%u",
                            tf.position.x, tf.position.z,
                            barr.queue.size(), barr.maxQueueSize);
                if (!barr.queue.empty()) {
                    const auto& job = barr.queue.front();
                    float pct = 1.f - (job.total > 0.f ? job.timer / job.total : 0.f);
                    ImGui::ProgressBar(pct, ImVec2(180, 0));
                }
                if (ImGui::Button("Ausbilden")) {
                    if (barr.queue.size() < barr.maxQueueSize) {
                        BarracksComponent::SpawnJob j;
                        j.tier  = 1;
                        j.total = 4.f;      // 4 s to produce
                        j.timer = j.total;
                        barr.queue.push_back(j);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Sofort (Cheat)")) {
                    // Bypass the queue: spawn one unit immediately at the building.
                    SpawnUnit(ctx, bc.teamId, tf.position + glm::vec3(2.f, 0.f, 0.f), bc.ownerClass);
                }
                ImGui::PopID();
            }
            if (!anyBarracks) {
                ImGui::TextDisabled("  Keine Brutkammer gebaut. (Bauen -> Brutkammer)");
                if (ImGui::Button("Test-Einheit am MainBase spawnen")) {
                    // Fallback: spawn near the team's Main Base so the user can
                    // try Commander mode without first placing a Barracks.
                    glm::vec3 sp{};
                    bool found = false;
                    auto mbView = ctx.serverRegistry.view<TransformComponent, BuildingComponent>();
                    for (auto mb : mbView) {
                        const auto& mbBc = mbView.get<BuildingComponent>(mb);
                        if (mbBc.teamId == myTeam && mbBc.type == BuildingType::Main && !mbBc.destroyed) {
                            sp = mbView.get<TransformComponent>(mb).position;
                            found = true;
                            break;
                        }
                    }
                    if (found) {
                        BugClass playerClass = BugClass::Ants;
                        auto pView = ctx.serverRegistry.view<PlayerComponent>();
                        for (auto pe : pView) {
                            auto& pc = pView.get<PlayerComponent>(pe);
                            if (pc.playerId == myTeam) {
                                if (pc.bugClass != BugClass::None)
                                    playerClass = pc.bugClass;
                                break;
                            }
                        }
                        SpawnUnit(ctx, myTeam, sp + glm::vec3(3.f, 0.f, 0.f), playerClass);
                    }
                }
            }
        }
    }



    if (ImGui::Button("Disconnect")) {
        ctx.network.Disconnect();
        ctx.scenes.RequestTransition(new MainMenuScene());
    }
    ImGui::End();

    // Resource stockpile HUD (top-right overlay). Both host and client read
    // from m_ClientBaseInventories — the host populates it locally during
    // SendInventoryUpdates, the client receives it via INVENTORY_UPDATE.
    // Find this player's main base by walking the client registry.
    {
        const ResourceInventory* invToShow = nullptr;
        auto bView = ctx.clientRegistry.view<BuildingComponent, NetworkedComponent>();
        for (auto e : bView) {
            const auto& bc = bView.get<BuildingComponent>(e);
            if (bc.teamId != m_MyPlayerId || bc.type != BuildingType::Main || bc.destroyed) continue;
            const auto& nc = bView.get<NetworkedComponent>(e);
            auto it = m_ClientBaseInventories.find(nc.netId);
            if (it != m_ClientBaseInventories.end()) {
                invToShow = &it->second;
                break;
            }
        }
        ResourceInventory empty{};
        ResourceHUD::DrawInventory(invToShow ? *invToShow : empty);
    }

    // Map overlay (available to all clients with synced data)
    {
        ImGui::Begin("Game");
        if (ImGui::Button(m_ShowMapOverlay ? "Karte schliessen" : "Karte [M]"))
            m_ShowMapOverlay = !m_ShowMapOverlay;
        ImGui::End();

        // Keyboard shortcut M
        if (ImGui::IsKeyPressed(ImGuiKey_M))
            m_ShowMapOverlay = !m_ShowMapOverlay;

        // Map overlay disabled for now — ingame 3D fog cover mesh handles visibility
        if (m_ShowMapOverlay) {
            // no-op; the ImGui per-cell overlay was replaced by the 3D fog cover mesh
        }
    }

    // HP bars above units (visible in both top-down modes)
    if (m_CameraMode == CameraMode::Commander || m_CameraMode == CameraMode::Building) {
        DrawUnitHPBars(ctx);
    }

    if (DebugUI::IsVisible()) {
        ImGui::Begin("Shadow Debug");
        ImGui::SliderFloat("Bias Constant", &m_ShadowBiasConstant, 0.0f, 10.0f);
        ImGui::SliderFloat("Bias Slope", &m_ShadowBiasSlope, 0.0f, 10.0f);
        ImGui::SliderFloat("Ortho Size", &m_ShadowOrthoSize, 5.0f, 100.0f);
        ImGui::End();
    }
}

/**
 * @brief Spawns a physics-driven cube in the world (Server only).
 * @param ctx The scene context.
 * @param pos Initial position.
 */
void GameScene::SpawnPhysicsCube(SceneContext& ctx, glm::vec3 pos) {
    if (!ctx.network.IsHosting()) return;

    const uint32_t cubeNetId = m_NextNetId++;
    auto cubeEntity = ctx.serverRegistry.create();
    
    uint32_t physicsId = ctx.world->GetNextPhysicsID();
    auto handle = ctx.physics->AddDynamicBox(
        physicsId,
        JPH::RVec3(pos.x, pos.y, pos.z),
        JPH::Vec3(1.f, 1.f, 1.f)
    );
    
    // Register the server entity first
    ctx.world->RegisterPhysicsEntity(physicsId, cubeEntity);
    
    ctx.serverRegistry.emplace<TransformComponent>(cubeEntity, pos);
    ctx.serverRegistry.emplace<MovementComponent>(cubeEntity);
    ctx.serverRegistry.emplace<PhysicsBodyComponent>(cubeEntity, handle);
    ctx.serverRegistry.emplace<NetworkedComponent>(cubeEntity, cubeNetId);
    ctx.serverRegistry.emplace<ModelComponent>(cubeEntity, "assets/cube.glb");
    m_ServerNetMap[cubeNetId] = cubeEntity;

    AssetJoinedPacket cubePkt;
    cubePkt.netId = cubeNetId;
    std::strncpy(cubePkt.modelPath, "assets/cube.glb", sizeof(cubePkt.modelPath)-1);
    cubePkt.x = pos.x; cubePkt.y = pos.y; cubePkt.z = pos.z;
    ctx.network.BroadcastToAll(cubePkt);

    spdlog::info("GameScene: spawned physics cube netId={} at ({}, {}, {})", 
                 cubeNetId, pos.x, pos.y, pos.z);
}

// ---------------------------------------------------------------------------
// Fixed-rate update
// ---------------------------------------------------------------------------

void GameScene::FixedUpdate(SceneContext& ctx, float dt) {
    if (ctx.network.IsHosting()) {
        PollConnectionEvents(ctx);
        PollClientPackets(ctx);

        // Process commander movement / attack orders from clients
        while (true) {
            auto result = ctx.network.ReceiveFromClient<CommanderOrderPacket>(PacketType::COMMANDER_ORDER);
            if (!result) break;
            auto [pkt, senderPeer] = *result;
            glm::vec3 dest{pkt.x, pkt.y, pkt.z};
            uint32_t count = std::min(pkt.selectedCount, 32u);

            if (pkt.orderType != 0) {
                // Attack order
                entt::entity targetEntity = entt::null;
                {
                    auto it = m_ServerNetMap.find(pkt.targetNetId);
                    if (it != m_ServerNetMap.end())
                        targetEntity = it->second;
                }

                if (targetEntity != entt::null && ctx.serverRegistry.valid(targetEntity)) {
                    auto* ttf = ctx.serverRegistry.try_get<TransformComponent>(targetEntity);
                    if (ttf) dest = ttf->position;
                }

                spdlog::info("Attack order playerId={}: targetNetId={} ({:.0f},{:.0f},{:.0f}) {} units",
                             pkt.playerId, pkt.targetNetId, dest.x, dest.y, dest.z, count);

                for (uint32_t i = 0; i < count; ++i) {
                    auto it = m_ServerNetMap.find(pkt.selectedNetIds[i]);
                    if (it == m_ServerNetMap.end()) continue;
                    entt::entity unit = it->second;

                    auto* cc = ctx.serverRegistry.try_get<CombatComponent>(unit);
                    if (cc) {
                        cc->target          = targetEntity;
                        cc->commandedTarget = true;
                    }

                    auto* mo = ctx.serverRegistry.try_get<MovementOrderComponent>(unit);
                    if (mo) {
                        mo->destination = dest;
                        mo->active      = true;
                    }

                    auto* path = ctx.serverRegistry.try_get<PathComponent>(unit);
                    if (path) path->dirty = true;
                }
            } else {
                // Move order — also clears any commanded attack target
                spdlog::info("Commander order playerId={}: ({:.1f},{:.1f},{:.1f}) {} units",
                             pkt.playerId, dest.x, dest.y, dest.z, count);

                for (uint32_t i = 0; i < count; ++i) {
                    auto it = m_ServerNetMap.find(pkt.selectedNetIds[i]);
                    if (it == m_ServerNetMap.end()) continue;
                    entt::entity unit = it->second;

                    auto* cc = ctx.serverRegistry.try_get<CombatComponent>(unit);
                    if (cc) {
                        cc->target          = entt::null;
                        cc->commandedTarget = false;
                    }

                    auto* mo = ctx.serverRegistry.try_get<MovementOrderComponent>(unit);
                    if (mo) {
                        mo->destination = dest;
                        mo->active      = true;
                    }

                    auto* path = ctx.serverRegistry.try_get<PathComponent>(unit);
                    if (path) path->dirty = true;
                }
            }
        }
    }

    if (ctx.network.IsConnected()) {
        PollServerPackets(ctx);
    }

    if (ctx.network.IsHosting()) {
        Systems::MovementSystem(ctx.serverRegistry, dt);
        UpdateUnitMovement(ctx, dt);
        UpdateCombat(ctx, dt);
        CheckWinCondition(ctx);
        m_ResourceManager.Update(ctx.serverRegistry, dt);
        ResourceSystem::Update(ctx.serverRegistry, m_ResourceManager, dt);
        BuildingSystem::Update(ctx.serverRegistry, dt);

        // Drain Barracks completed-spawn flags into real Unit entities.
        // BuildingSystem can't do this itself because it doesn't know about
        // networking or our SpawnUnit helper.
        {
            auto bv = ctx.serverRegistry.view<TransformComponent,
                                              BuildingComponent,
                                              BarracksComponent>();
            for (auto e : bv) {
                auto& barr = bv.get<BarracksComponent>(e);
                if (barr.completedSpawns <= 0) continue;
                const auto& tf = bv.get<TransformComponent>(e);
                const auto& bc = bv.get<BuildingComponent>(e);
                for (int i = 0; i < barr.completedSpawns; ++i) {
                    // Spawn just in front of the barracks (offset on X) so units
                    // don't pile up exactly on top of the building.
                    glm::vec3 p = tf.position + glm::vec3((float)i * 1.4f + 2.f, 0.f, 0.f);
                    SpawnUnit(ctx, bc.teamId, p, bc.ownerClass);
                }
                barr.completedSpawns = 0;
            }
        }
        // Update fog for each team separately (units reveal only for their own team)
        {
            bool anyChanged = false;
            for (auto& [teamId, fg] : m_TeamFogs) {
                if (FogOfWarSystem::UpdateForTeam(fg, ctx.serverRegistry, teamId))
                    anyChanged = true;
            }
            if (anyChanged) {
                m_ScatterBatchesDirty = true;
            }
        }
        TerritorySystem::Update(ctx.serverRegistry, dt);

        // Sync resource state changes to clients (new spawns, depletions, respawns)
        SyncResourceSpawns(ctx);
    }

    SendLocalInput(ctx);

    if (ctx.network.IsHosting()) {
        // Entity transform snapshots (20 Hz)
        m_SnapAccum += dt;
        if (m_SnapAccum >= SNAPSHOT_RATE) {
            SendSnapshots(ctx);
            m_SnapAccum -= SNAPSHOT_RATE;
        }

        // Territory state snapshots (2 Hz)
        m_TerritorySnapAccum += dt;
        if (m_TerritorySnapAccum >= TERRITORY_SNAP_RATE) {
            SendTerritorySnapshot(ctx);
            m_TerritorySnapAccum -= TERRITORY_SNAP_RATE;
        }

        // Fog-of-war snapshots (2 Hz)
        m_FogSnapAccum += dt;
        if (m_FogSnapAccum >= FOG_SNAP_RATE) {
            SendFogSnapshot(ctx);
            m_FogSnapAccum -= FOG_SNAP_RATE;
        }

        // Per-base resource inventory snapshots (2 Hz). Also mirrored to the
        // host's own m_ClientBaseInventories map so the HUD reads from one
        // place on both host and joined clients.
        m_InventorySnapAccum += dt;
        if (m_InventorySnapAccum >= INVENTORY_SNAP_RATE) {
            SendInventoryUpdates(ctx);
            m_InventorySnapAccum -= INVENTORY_SNAP_RATE;
        }
    }

    // Separate throttle for scatter batch rebuilds (delayed, less frequent)
    if (m_ScatterBatchesDirty) {
        m_ScatterBatchTimer += dt;
        if (m_ScatterBatchTimer >= SCATTER_BATCH_REBUILD_DELAY) {
            const FogGrid& f = LocalFog(ctx);
            BuildScatterBatches(ctx, f.IsInitialised() ? &f : nullptr);
            m_ScatterBatchesDirty = false;
            m_ScatterBatchTimer = 0.f;
            // Rebuild the GPU fog texture at the same throttle rate
            m_FogTextureDirty = true;
        }
    } else {
        m_ScatterBatchTimer = 0.f;
    }
}

// ---------------------------------------------------------------------------
// Server-side: connection / disconnection
// ---------------------------------------------------------------------------

void GameScene::PollConnectionEvents(SceneContext& ctx) {
    uint32_t peerId;
    bool     isConnect;

    while (ctx.network.PollPlayerConnection(peerId, isConnect)) {
        if (isConnect) {
            const uint32_t newNetId    = m_NextNetId++;
            const uint32_t newPlayerId = m_NextPlayerId++;

            // Sync existing entities to the new client
            for (auto& [netId, ent] : m_ServerNetMap) {
                auto& t = ctx.serverRegistry.get<TransformComponent>(ent);
                
                auto* p = ctx.serverRegistry.try_get<PlayerComponent>(ent);
                if (p) {
                    PlayerJoinedPacket pkt;
                    pkt.netId    = netId;
                    pkt.playerId = p->playerId;
                    pkt.x = t.position.x; pkt.y = t.position.y; pkt.z = t.position.z;
                    ctx.network.SendToClient(peerId, pkt);
                }

                auto* m = ctx.serverRegistry.try_get<ModelComponent>(ent);
                if (m) {
                    // If this entity is a building, send the building-specific packet
                    auto* bc = ctx.serverRegistry.try_get<BuildingComponent>(ent);
                    if (bc) {
                        BuildingSpawnedPacket bp;
                        bp.netId        = netId;
                        bp.teamId       = bc->teamId;
                        bp.buildingType = static_cast<uint8_t>(bc->type);
                        bp.specialType  = static_cast<uint8_t>(bc->specialType);
                        bp.tier         = bc->tier;
                        bp.x = t.position.x; bp.y = t.position.y; bp.z = t.position.z;
                        bp.hp = bc->hp; bp.maxHp = bc->maxHp;
                        ctx.network.SendToClient(peerId, bp);
                    } else {
                        AssetJoinedPacket pkt;
                        pkt.netId = netId;
                        std::strncpy(pkt.modelPath, m->modelPath.c_str(), sizeof(pkt.modelPath)-1);
                        pkt.x = t.position.x; pkt.y = t.position.y; pkt.z = t.position.z;
                        ctx.network.SendToClient(peerId, pkt);
                    }
                }
            }

            // Sync all resource nodes to the new client
            for (auto& [resNetId, resEnt] : m_ResourceNetMap) {
                if (!ctx.serverRegistry.valid(resEnt)) continue;
                auto& res = ctx.serverRegistry.get<ResourceComponent>(resEnt);
                auto& tf  = ctx.serverRegistry.get<TransformComponent>(resEnt);
                ResourceSpawnedPacket rpkt;
                rpkt.netId = resNetId;
                rpkt.resourceType = static_cast<uint8_t>(res.type);
                rpkt.x = tf.position.x; rpkt.y = tf.position.y; rpkt.z = tf.position.z;
                ctx.network.SendToClient(peerId, rpkt);
            }

            // Sync completed upgrades for this player's team to the new client
            {
                uint32_t teamId = newPlayerId;
                auto& state = UpgradeSystem::GetState(teamId);
                for (auto pid : state.completedPaths) {
                    UpgradeCompletedPacket upkt;
                    upkt.pathId = pid;
                    upkt.teamId = teamId;
                    ctx.network.SendToClient(peerId, upkt);
                }
            }

            // Ensure a fog grid exists for the new player's team
            if (m_TeamFogs.find(newPlayerId) == m_TeamFogs.end()) {
                FogGrid fg;
                fg.Init(glm::vec3{-375.f, 0.f, -375.f},
                        glm::vec3{ 375.f, 0.f,  375.f},
                        /*cellSize=*/1.f);
                m_TeamFogs[newPlayerId] = std::move(fg);
            }

            // Send initial territory + fog snapshots to the new client.
            // Use a full snapshot (not delta) so the client initializes its fog state.
            SendTerritorySnapshot(ctx);
            {
                auto& fg = m_TeamFogs[newPlayerId];
                if (fg.IsInitialised()) {
                    FogSnapshotPacket fpkt;
                    fpkt.cellsX = static_cast<uint16_t>(fg.cellsX);
                    fpkt.cellsZ = static_cast<uint16_t>(fg.cellsZ);
                    size_t total = static_cast<size_t>(fg.cellsX) * fg.cellsZ;
                    size_t words = (total + 63) / 64;
                    if (words > 2200) words = 2200;
                    for (size_t w = 0; w < words; ++w) {
                        uint64_t bits = 0;
                        for (size_t b = 0; b < 64; ++b) {
                            size_t idx = w * 64 + b;
                            if (idx < total && fg.revealed[idx])
                                bits |= (uint64_t(1) << b);
                        }
                        fpkt.gridData[w] = bits;
                    }
                    ctx.network.SendToClient(peerId, fpkt);
                }
            }

            // Tell the new client their identity
            PlayerIdAssignPacket idPkt;
            idPkt.playerId = newPlayerId;
            idPkt.netId    = newNetId;
            ctx.network.SendToClient(peerId, idPkt);

            // Create the server entity
            auto entity = ctx.serverRegistry.create();
            ctx.serverRegistry.emplace<TransformComponent>(entity);
            ctx.serverRegistry.emplace<MovementComponent>(entity);
            // Stamp peerId so the mapping survives scene transitions.
            PlayerComponent pc{};
            pc.playerId = newPlayerId;
            pc.isLocal  = false;
            pc.peerId   = peerId;
            ctx.serverRegistry.emplace<PlayerComponent>(entity, pc);
            ctx.serverRegistry.emplace<NetworkedComponent>(entity, newNetId);
            m_ServerNetMap[newNetId] = entity;
            m_PeerToNetId[peerId]    = newNetId;

            // Broadcast the new player to all clients
            PlayerJoinedPacket broadcastPkt;
            broadcastPkt.netId    = newNetId;
            broadcastPkt.playerId = newPlayerId;
            ctx.network.BroadcastToAll(broadcastPkt);

            spdlog::info("GameScene: player joined  peerId={} playerId={} netId={}",
                         peerId, newPlayerId, newNetId);
        } else {
            auto it = m_PeerToNetId.find(peerId);
            if (it != m_PeerToNetId.end()) {
                const uint32_t netId = it->second;
                ctx.serverRegistry.destroy(m_ServerNetMap[netId]);
                m_ServerNetMap.erase(netId);
                m_PeerToNetId.erase(peerId);
                PlayerLeftPacket pkt;
                pkt.netId = netId;
                ctx.network.BroadcastToAll(pkt);
                spdlog::info("GameScene: player left  peerId={} netId={}", peerId, netId);
            }
        }
    }
}

void GameScene::PollClientPackets(SceneContext& ctx) {
    while (true) {
        auto result = ctx.network.ReceiveFromClient<PlayerInputPacket>(PacketType::PLAYER_INPUT);
        if (!result) break;
        auto [packet, senderPeerId] = *result;
        auto peerIt = m_PeerToNetId.find(senderPeerId);
        if (peerIt == m_PeerToNetId.end()) continue;
        auto entIt = m_ServerNetMap.find(peerIt->second);
        if (entIt == m_ServerNetMap.end()) continue;
        auto* movement = ctx.serverRegistry.try_get<MovementComponent>(entIt->second);
        if (movement) movement->inputDir = {packet.dx, packet.dy, packet.dz};
        auto* transform = ctx.serverRegistry.try_get<TransformComponent>(entIt->second);
        if (transform) {
            transform->rotation.y = packet.yaw;
            transform->rotation.x = packet.pitch;
        }
    }
}

void GameScene::SendSnapshots(SceneContext& ctx) {
    auto view = ctx.serverRegistry.view<NetworkedComponent, TransformComponent>();
    for (auto entity : view) {
        auto& net = view.get<NetworkedComponent>(entity);
        auto& t   = view.get<TransformComponent>(entity);
        EntitySnapshotPacket pkt;
        pkt.netId = net.netId;
        pkt.x = t.position.x; pkt.y = t.position.y; pkt.z = t.position.z;
        pkt.rx = t.rotation.x; pkt.ry = t.rotation.y; pkt.rz = t.rotation.z;
        pkt.sx = t.scale.x;    pkt.sy = t.scale.y;    pkt.sz = t.scale.z;
        auto* m = ctx.serverRegistry.try_get<MovementComponent>(entity);
        if (m) {
            pkt.vx = m->velocity.x; pkt.vy = m->velocity.y; pkt.vz = m->velocity.z;
        } else {
            pkt.vx = pkt.vy = pkt.vz = 0.f;
        }
        ctx.network.BroadcastToAll(pkt);
    }
}

void GameScene::PollServerPackets(SceneContext& ctx) {
    if (!m_IdAssigned) {
        auto idPkt = ctx.network.ReceiveFromServer<PlayerIdAssignPacket>(PacketType::PLAYER_ID_ASSIGN);
        if (idPkt) {
            m_MyPlayerId = idPkt->playerId;
            m_MyNetId    = idPkt->netId;
            m_IdAssigned = true;
            spdlog::info("GameScene: assigned playerId={} netId={}", m_MyPlayerId, m_MyNetId);
        }
    }

    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<PlayerJoinedPacket>(PacketType::PLAYER_JOINED);
        if (!pkt) break;
        if (m_ClientNetMap.count(pkt->netId)) continue;
        auto entity = ctx.clientRegistry.create();
        ctx.clientRegistry.emplace<TransformComponent>(entity, glm::vec3{pkt->x, pkt->y, pkt->z});
        ctx.clientRegistry.emplace<MovementComponent>(entity);
        ctx.clientRegistry.emplace<PlayerComponent>(entity, pkt->playerId, pkt->playerId == m_MyPlayerId);
        ctx.clientRegistry.emplace<NetworkedComponent>(entity, pkt->netId);
        m_ClientNetMap[pkt->netId] = entity;
    }

    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<PlayerLeftPacket>(PacketType::PLAYER_LEFT);
        if (!pkt) break;
        auto it = m_ClientNetMap.find(pkt->netId);
        if (it != m_ClientNetMap.end()) {
            ctx.clientRegistry.destroy(it->second);
            m_ClientNetMap.erase(it);
        }
    }

    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<EntitySnapshotPacket>(PacketType::ENTITY_SNAPSHOT);
        if (!pkt) break;
        auto it = m_ClientNetMap.find(pkt->netId);
        if (it == m_ClientNetMap.end()) continue;

        auto* t = ctx.clientRegistry.try_get<TransformComponent>(it->second);
        auto* m = ctx.clientRegistry.try_get<MovementComponent>(it->second);
        if (t) {
            bool isLocalPlayer = false;
            if (auto* p = ctx.clientRegistry.try_get<PlayerComponent>(it->second)) {
                if (p->isLocal) isLocalPlayer = true;
            }
            // Don't override position for buildings under construction
            if (!ctx.clientRegistry.try_get<ConstructionComponent>(it->second)) {
                t->position = glm::vec3{pkt->x, pkt->y, pkt->z};
            }
            if (!isLocalPlayer) t->rotation = glm::vec3{pkt->rx, pkt->ry, pkt->rz};
            t->scale = glm::vec3{pkt->sx, pkt->sy, pkt->sz};
        }
        if (m) m->velocity = {pkt->vx, pkt->vy, pkt->vz};
    }

    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<AssetJoinedPacket>(PacketType::ASSET_JOINED);
        if (!pkt) break;
        if (m_ClientNetMap.count(pkt->netId)) continue;
        auto entity = ctx.clientRegistry.create();
        ctx.clientRegistry.emplace<TransformComponent>(entity, glm::vec3{pkt->x, pkt->y, pkt->z});
        ctx.clientRegistry.emplace<NetworkedComponent>(entity, pkt->netId);
        ctx.clientRegistry.emplace<ModelComponent>(entity, std::string(pkt->modelPath));
        m_ClientNetMap[pkt->netId] = entity;
    }

    // Unit spawned
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<UnitSpawnedPacket>(PacketType::UNIT_SPAWNED);

        if (!pkt) break;
        if (m_ClientNetMap.count(pkt->netId)) continue;
        const UnitRole role = (pkt->role == 1) ? UnitRole::Worker : UnitRole::Combat;
        auto entity = ctx.clientRegistry.create();
        auto& tf = ctx.clientRegistry.emplace<TransformComponent>(entity, glm::vec3{pkt->x, pkt->y, pkt->z});
        if (role == UnitRole::Worker) tf.scale = glm::vec3(0.6f); // visibly smaller than combat units
        ctx.clientRegistry.emplace<MovementComponent>(entity);
        ctx.clientRegistry.emplace<NetworkedComponent>(entity, pkt->netId);
        ctx.clientRegistry.emplace<ModelComponent>(entity, std::string("assets/cube.glb"));
        ctx.clientRegistry.emplace<UnitComponent>(entity,
            UnitComponent{pkt->teamId, static_cast<BugClass>(pkt->bugClass), false, role});
        ctx.clientRegistry.emplace<HealthComponent>(entity,
            HealthComponent{pkt->maxHp});
        ctx.clientRegistry.emplace<MovementOrderComponent>(entity);
        m_ClientNetMap[pkt->netId] = entity;
    }

    // Unit died
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<UnitDiedPacket>(PacketType::UNIT_DIED);
        if (!pkt) break;
        auto it = m_ClientNetMap.find(pkt->netId);
        if (it == m_ClientNetMap.end()) continue;
        ctx.clientRegistry.destroy(it->second);
        m_ClientNetMap.erase(it);
        // Remove from selection if present
        m_SelectedUnits.erase(std::remove(m_SelectedUnits.begin(), m_SelectedUnits.end(), pkt->netId),
                               m_SelectedUnits.end());
    }

    // HP update
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<UnitHpUpdatePacket>(PacketType::UNIT_HP_UPDATE);
        if (!pkt) break;
        auto it = m_ClientNetMap.find(pkt->netId);
        if (it == m_ClientNetMap.end()) continue;
        auto* hc = ctx.clientRegistry.try_get<HealthComponent>(it->second);
        if (hc) hc->hp = pkt->hp;
    }

    // Territory snapshot
    {
        auto pkt = ctx.network.ReceiveFromServer<TerritorySnapshotPacket>(PacketType::TERRITORY_SNAPSHOT);
        if (pkt) HandleTerritorySnapshot(*pkt);
    }

    // Fog delta (incremental updates)
    {
        while (true) {
            auto pkt = ctx.network.ReceiveFromServer<FogDeltaPacket>(PacketType::FOG_DELTA);
            if (!pkt) break;
            if (!m_HasFogData) {
                // Initialise client fog grid with extents matching the server
                m_ClientFog.Init(
                    glm::vec3{-375.f, 0.f, -375.f},
                    glm::vec3{ 375.f, 0.f,  375.f},
                    1.f
                );
                m_HasFogData = true;
            }
            if (m_ClientFog.ApplyDelta(pkt->cells, pkt->count)) {
                m_ScatterBatchesDirty = true;
            }
        }
    }
    // Full fog snapshot (initial sync or fallback)
    {
        auto pkt = ctx.network.ReceiveFromServer<FogSnapshotPacket>(PacketType::FOG_SNAPSHOT);
        if (pkt) {
            HandleFogSnapshot(*pkt);
            m_ScatterBatchesDirty = true;
        }
    }

    // Resource spawned
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<ResourceSpawnedPacket>(PacketType::RESOURCE_SPAWNED);
        if (!pkt) break;
        if (m_ClientNetMap.count(pkt->netId)) continue;
        // Per-type visual: use one of the already-loaded scatter GLBs so each
        // resource type reads at a glance instead of all being identical cubes.
        const auto rtype = static_cast<ResourceType>(pkt->resourceType);
        std::string modelPath = "assets/cube.glb";
        switch (rtype) {
            case ResourceType::Pilze:    modelPath = "assets/Mushroom_2.glb";     break;
            case ResourceType::Beeren:   modelPath = "assets/Watermelon_1.glb";   break;
            case ResourceType::Nektar:   modelPath = "assets/Plant_5.glb";        break;
            case ResourceType::Samen:    modelPath = "assets/Wheat_2.glb";        break;
            case ResourceType::Insekten: modelPath = "assets/prim_sphere_red.glb";break;
            case ResourceType::Fleisch:  modelPath = "assets/prim_sphere_red.glb";break;
            case ResourceType::Holz:     modelPath = "assets/WoodLog.glb";        break;
            default: break;
        }
        auto entity = ctx.clientRegistry.create();
        ctx.clientRegistry.emplace<TransformComponent>(entity, glm::vec3{pkt->x, pkt->y, pkt->z});
        ctx.clientRegistry.emplace<NetworkedComponent>(entity, pkt->netId);
        ctx.clientRegistry.emplace<ModelComponent>(entity, modelPath);
        m_ClientNetMap[pkt->netId] = entity;
    }

    // Resource depleted
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<ResourceDepletedPacket>(PacketType::RESOURCE_DEPLETED);
        if (!pkt) break;
        auto it = m_ClientNetMap.find(pkt->netId);
        if (it != m_ClientNetMap.end()) {
            ctx.clientRegistry.destroy(it->second);
            m_ClientNetMap.erase(it);
        }
    }

    // Inventory snapshots (per-base resource stockpile, host → clients @ 2 Hz)
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<InventoryUpdatePacket>(PacketType::INVENTORY_UPDATE);
        if (!pkt) break;
        ResourceInventory& inv = m_ClientBaseInventories[pkt->baseNetId];
        inv.pilze    = pkt->pilze;
        inv.beeren   = pkt->beeren;
        inv.nektar   = pkt->nektar;
        inv.samen    = pkt->samen;
        inv.insekten = pkt->insekten;
        inv.fleisch  = pkt->fleisch;
        inv.holz     = pkt->holz;
    }

    // Game over
    {
        auto pkt = ctx.network.ReceiveFromServer<GameOverPacket>(PacketType::GAME_OVER);
        if (pkt && !m_GameOver) {
            m_GameOver      = true;
            m_GameOverTimer = 0.f;
            m_WinnerTeam    = pkt->winnerTeam;
            spdlog::info("GameScene: GAME OVER – winner team {}", m_WinnerTeam);
        }
    }

    // Return to lobby (host signal)
    {
        auto pkt = ctx.network.ReceiveFromServer<ReturnToLobbyPacket>(PacketType::RETURN_TO_LOBBY);
        if (pkt) {
            spdlog::info("GameScene: host returned everyone to lobby");
            ctx.scenes.RequestTransition(new LobbyScene());
        }
    }

    // Building spawned
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<BuildingSpawnedPacket>(PacketType::BUILDING_SPAWNED);
        if (!pkt) break;
        if (m_ClientNetMap.count(pkt->netId)) continue;
        auto entity = ctx.clientRegistry.create();
        // Start slightly below ground for construction slide-up animation
        float targetY = pkt->y;
        float startY  = targetY - 0.5f;
        auto bType = static_cast<BuildingType>(pkt->buildingType);
        auto sType = static_cast<SpecialBuildingType>(pkt->specialType);
        ctx.clientRegistry.emplace<TransformComponent>(entity, glm::vec3{pkt->x, startY, pkt->z});
        ctx.clientRegistry.emplace<NetworkedComponent>(entity, pkt->netId);
        ctx.clientRegistry.emplace<ModelComponent>(entity, std::string("assets/cube.glb"));
        ctx.clientRegistry.emplace<BuildingComponent>(entity,
            BuildingComponent{bType, pkt->teamId, pkt->tier, sType, pkt->hp, pkt->maxHp, false});
        if (bType == BuildingType::Special)
            ctx.clientRegistry.emplace<SpecialBuildingComponent>(entity, sType);
        ctx.clientRegistry.emplace<ConstructionComponent>(entity,
            ConstructionComponent{0.f, 1.0f, startY, targetY});
        // Add a HealthComponent so existing HP-update packet handling works
        auto& hc = ctx.clientRegistry.emplace<HealthComponent>(entity, HealthComponent{pkt->maxHp});
        hc.hp = pkt->hp;
        m_ClientNetMap[pkt->netId] = entity;

        // Position camera at local player's Main Base
        if (bType == BuildingType::Main && pkt->teamId == m_MyPlayerId) {
            m_Camera->m_Position = glm::vec3(pkt->x, m_TopDownHeight, pkt->z);
            m_Camera->UpdateVectors();
            spdlog::info("GameScene: camera positioned at Main Base ({:.1f}, {:.1f})",
                         pkt->x, pkt->z);
        }
    }

    // Building destroyed
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<BuildingDestroyedPacket>(PacketType::BUILDING_DESTROYED);
        if (!pkt) break;
        auto it = m_ClientNetMap.find(pkt->netId);
        if (it == m_ClientNetMap.end()) continue;
        ctx.clientRegistry.destroy(it->second);
        m_ClientNetMap.erase(it);
    }

    // Upgrade completed
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<UpgradeCompletedPacket>(PacketType::UPGRADE_COMPLETED);
        if (!pkt) break;
        // Just log it client-side for now; effects are handled server-authoritative
        spdlog::info("GameScene: team {} completed upgrade path {} (client)",
                     pkt->teamId, pkt->pathId);
    }
}

void GameScene::SendLocalInput(SceneContext& ctx) {
    if (!ctx.network.IsConnected() || !Input::IsRelativeMouseMode() || !m_Camera) return;
    glm::vec3 forward = m_Camera->m_Front;
    forward.y = 0.f;
    if (glm::length(forward) > 0.0001f) forward = glm::normalize(forward);
    glm::vec3 right = m_Camera->m_Right;
    right.y = 0.f;
    if (glm::length(right) > 0.0001f) right = glm::normalize(right);
    glm::vec3 moveDir{0.f};
    if (Input::IsKeyDown(SDLK_W)) moveDir += forward;
    if (Input::IsKeyDown(SDLK_S)) moveDir -= forward;
    if (Input::IsKeyDown(SDLK_A)) moveDir -= right;
    if (Input::IsKeyDown(SDLK_D)) moveDir += right;
    if (Input::IsKeyDown(SDLK_SPACE)) moveDir.y += 1.f;
    if (Input::IsKeyDown(SDLK_LSHIFT)) moveDir.y -= 1.f;
    if (glm::length(moveDir) > 0.f) moveDir = glm::normalize(moveDir);

    PlayerInputPacket pkt;
    pkt.dx = moveDir.x; pkt.dy = moveDir.y; pkt.dz = moveDir.z;
    pkt.yaw = m_Camera->m_Yaw; pkt.pitch = m_Camera->m_Pitch;
    ctx.network.Send(pkt);
}

// ---------------------------------------------------------------------------
// Mesh Collision
// ---------------------------------------------------------------------------

/**
 * @brief Loads scene geometry as static Jolt MeshShape collision bodies.
 *
 * Workflow:
 *  1. Load (or retrieve from cache) the GLTF scene via AssetManager.
 *  2. Extract CPU-side vertices + indices from the cached SceneData.
 *  3. Build a Jolt MeshShape using MeshCollisionBuilder.
 *  4. Register the resulting body with the PhysicsServer.
 *  5. Store the handle in m_MeshCollisionBodies for cleanup on OnExit().
 *
 * The function logs a warning and returns gracefully if the physics server
 * is unavailable, the asset fails to load, or the shape cannot be created.
 *
 * @param ctx       Scene context providing access to the PhysicsServer.
 * @param glbPath   Path to the .glb file whose geometry is used for collision.
 * @param transform Optional world-space transform baked into the shape vertices.
 */
void GameScene::LoadSceneMeshCollision(
    SceneContext&      ctx,
    const std::string& glbPath,
    const glm::mat4&   transform)
{
    if (!ctx.physics) {
        spdlog::warn("GameScene::LoadSceneMeshCollision: no PhysicsServer available.");
        return;
    }

    // 1. Load (or retrieve cached) GLTF scene data including CPU vertices/indices.
    SceneData sceneData = AssetManager::LoadGLTF(glbPath);
    if (!sceneData.model) {
        spdlog::error("GameScene::LoadSceneMeshCollision: failed to load '{}'", glbPath);
        return;
    }

    if (sceneData.cpuVertices.empty() || sceneData.cpuIndices.empty()) {
        spdlog::error("GameScene::LoadSceneMeshCollision: '{}' has no CPU mesh data.", glbPath);
        return;
    }

    spdlog::info("GameScene::LoadSceneMeshCollision: building mesh shape for '{}' "
                 "({} vertices, {} indices)",
                 glbPath,
                 sceneData.cpuVertices.size(),
                 sceneData.cpuIndices.size());

    // 2. Build the Jolt MeshShape from CPU geometry.
    JPH::Shape::ShapeResult result = MeshCollisionBuilder::Build(
        sceneData.cpuVertices,
        sceneData.cpuIndices,
        transform
    );

    if (!result.IsValid()) {
        spdlog::error("GameScene::LoadSceneMeshCollision: MeshShape creation failed for '{}': {}",
                      glbPath, result.GetError().c_str());
        return;
    }

    // 3. Register the static mesh body with the physics server.
    PhysicsBodyHandle handle = ctx.physics->AddStaticMesh(
        result.Get(),
        JPH::RVec3::sZero(),
        JPH::Quat::sIdentity()
    );

    if (!handle.IsValid()) {
        spdlog::error("GameScene::LoadSceneMeshCollision: AddStaticMesh failed for '{}'", glbPath);
        return;
    }

    // 4. Store for later cleanup.
    m_MeshCollisionBodies.push_back(handle);

    spdlog::info("GameScene::LoadSceneMeshCollision: mesh collision active for '{}'", glbPath);
}

// ---------------------------------------------------------------------------
// SpawnUnit
// ---------------------------------------------------------------------------

/**
 * @brief Spawns a unit on the server, assigns components and broadcasts.
 *
 * Damage stats are derived from BugClass:
 *   Carnivores (Mantis, Dragonflies, Scorpions) → high damage (20)
 *   Omnivores  (Ants, Roaches, Beetles, CentipedesWorms) → medium (12)
 *   Rest       → low (7)
 */
void GameScene::SpawnUnit(SceneContext& ctx, uint32_t teamId, glm::vec3 pos, BugClass bc, float hp)
{
    if (!ctx.network.IsHosting()) return;

    // Position: ground units sit on terrain, flying units hover above it.
    pos.y = GroundHeightAt(m_World, pos.x, pos.z);
    if (IsFlying(bc))
        pos.y += 4.f;

    // Damage by diet archetype
    float dmg = 7.f;
    if (bc == BugClass::Mantis || bc == BugClass::Dragonflies || bc == BugClass::Scorpions)
        dmg = 20.f;
    else if (bc == BugClass::Ants || bc == BugClass::Roaches || bc == BugClass::Beetles ||
             bc == BugClass::CentipedesWorms)
        dmg = 12.f;

    // Debuffs
    if (IsFlying(bc))
        hp *= 0.75f;
    float speed = IsClimber(bc) ? 6.f : 10.f;

    const uint32_t netId = m_NextNetId++;
    auto e = ctx.serverRegistry.create();
    ctx.serverRegistry.emplace<TransformComponent>(e, pos);
    ctx.serverRegistry.emplace<MovementComponent>(e, MovementComponent{glm::vec3(0.f), speed});
    ctx.serverRegistry.emplace<NetworkedComponent>(e, netId);
    ctx.serverRegistry.emplace<ModelComponent>(e, std::string("assets/cube.glb"));
    ctx.serverRegistry.emplace<UnitComponent>(e, UnitComponent{teamId, bc, false});
    ctx.serverRegistry.emplace<HealthComponent>(e, HealthComponent{hp});
    ctx.serverRegistry.emplace<CombatComponent>(e,
        CombatComponent{/*range=*/6.f, /*dmg=*/dmg, /*cd=*/0.f, /*rate=*/1.5f, entt::null, false});
    ctx.serverRegistry.emplace<MovementOrderComponent>(e);
    m_ServerNetMap[netId] = e;

    UnitSpawnedPacket pkt;
    pkt.netId    = netId;
    pkt.teamId   = teamId;
    pkt.bugClass = static_cast<uint8_t>(bc);
    pkt.x = pos.x; pkt.y = pos.y; pkt.z = pos.z;
    pkt.hp = hp; pkt.maxHp = hp;
    ctx.network.BroadcastToAll(pkt);

    // Also create the client-side entity directly when hosting, because
    // BroadcastToAll doesn't loop back to the host's local client — so
    // without this the host never sees its own freshly spawned units.
    // Mirrors the same fallback that SpawnBuilding uses.
    if (!m_ClientNetMap.count(netId)) {
        auto ce = ctx.clientRegistry.create();
        ctx.clientRegistry.emplace<TransformComponent>(ce, pos);
        ctx.clientRegistry.emplace<MovementComponent>(ce, MovementComponent{glm::vec3(0.f), speed});
        ctx.clientRegistry.emplace<NetworkedComponent>(ce, netId);
        ctx.clientRegistry.emplace<ModelComponent>(ce, std::string("assets/cube.glb"));
        ctx.clientRegistry.emplace<UnitComponent>(ce, UnitComponent{teamId, bc, false});
        ctx.clientRegistry.emplace<HealthComponent>(ce, HealthComponent{hp});
        ctx.clientRegistry.emplace<MovementOrderComponent>(ce);
        m_ClientNetMap[netId] = ce;
    }

    spdlog::info("GameScene: spawned unit netId={} team={} class={} hp={:.0f}",
                 netId, teamId, (int)bc, hp);
}

// ---------------------------------------------------------------------------
// SpawnWorker — a scaled-down combat unit specialised for resource gathering.
// Same model as a normal unit (faction's base unit), inherits the bug class's
// movement archetype (flyer / climber / walker), but with:
//   * Transform.scale = 0.6 so it's visibly distinct
//   * CombatComponent at reduced damage (workers can defend themselves)
//   * CollectorComponent + ResourceInventory so the existing ResourceSystem
//     state machine picks it up
//   * UnitRole::Worker so the UI, AI and pricing code can branch on it
// ---------------------------------------------------------------------------
void GameScene::SpawnWorker(SceneContext& ctx, uint32_t teamId, glm::vec3 pos, BugClass bc, float hp)
{
    if (!ctx.network.IsHosting()) return;

    pos.y = GroundHeightAt(m_World, pos.x, pos.z);
    if (IsFlying(bc)) pos.y += 4.f;

    float speed = IsClimber(bc) ? 6.f : 10.f;

    const uint32_t netId = m_NextNetId++;
    auto e = ctx.serverRegistry.create();
    auto& tf = ctx.serverRegistry.emplace<TransformComponent>(e, pos);
    tf.scale = glm::vec3(0.6f);
    ctx.serverRegistry.emplace<MovementComponent>(e, MovementComponent{glm::vec3(0.f), speed});
    ctx.serverRegistry.emplace<NetworkedComponent>(e, netId);
    ctx.serverRegistry.emplace<ModelComponent>(e, std::string("assets/cube.glb"));
    ctx.serverRegistry.emplace<UnitComponent>(e, UnitComponent{teamId, bc, false, UnitRole::Worker});
    ctx.serverRegistry.emplace<HealthComponent>(e, HealthComponent{hp});
    // Workers can defend themselves but hit weakly — they're economic units, not soldiers.
    ctx.serverRegistry.emplace<CombatComponent>(e,
        CombatComponent{/*range=*/4.f, /*dmg=*/3.f, /*cd=*/0.f, /*rate=*/2.0f, entt::null, false});
    ctx.serverRegistry.emplace<MovementOrderComponent>(e);
    ctx.serverRegistry.emplace<CollectorComponent>(e);
    ctx.serverRegistry.emplace<ResourceInventory>(e);
    m_ServerNetMap[netId] = e;

    UnitSpawnedPacket pkt;
    pkt.netId    = netId;
    pkt.teamId   = teamId;
    pkt.bugClass = static_cast<uint8_t>(bc);
    pkt.role     = 1; // Worker
    pkt.x = pos.x; pkt.y = pos.y; pkt.z = pos.z;
    pkt.hp = hp; pkt.maxHp = hp;
    ctx.network.BroadcastToAll(pkt);

    // Host-local client mirror (BroadcastToAll doesn't loop back to host).
    if (!m_ClientNetMap.count(netId)) {
        auto ce = ctx.clientRegistry.create();
        auto& ctf = ctx.clientRegistry.emplace<TransformComponent>(ce, pos);
        ctf.scale = glm::vec3(0.6f);
        ctx.clientRegistry.emplace<MovementComponent>(ce, MovementComponent{glm::vec3(0.f), speed});
        ctx.clientRegistry.emplace<NetworkedComponent>(ce, netId);
        ctx.clientRegistry.emplace<ModelComponent>(ce, std::string("assets/cube.glb"));
        ctx.clientRegistry.emplace<UnitComponent>(ce, UnitComponent{teamId, bc, false, UnitRole::Worker});
        ctx.clientRegistry.emplace<HealthComponent>(ce, HealthComponent{hp});
        ctx.clientRegistry.emplace<MovementOrderComponent>(ce);
        m_ClientNetMap[netId] = ce;
    }

    spdlog::info("GameScene: spawned WORKER netId={} team={} class={} hp={:.0f}",
                 netId, teamId, (int)bc, hp);
}

// ---------------------------------------------------------------------------
// SpawnBuilding
// ---------------------------------------------------------------------------

entt::entity GameScene::SpawnBuilding(SceneContext& ctx, BuildingType type,
                                       uint32_t teamId, glm::vec3 pos,
                                       uint32_t tier, const std::string& model,
                                       SpecialBuildingType specialType)
{
    if (!ctx.network.IsHosting()) return entt::null;

    // Snap to the terrain surface so buildings rest on the terraced ground
    // instead of the old flat Y=0 plane. Authoritative on the server; the
    // broadcast carries this Y to every client.
    pos.y = GroundHeightAt(m_World, pos.x, pos.z);

    // Determine the owner's bug class from the first player of this team
    BugClass ownerClass = BugClass::Termites;
    {
        auto pView = ctx.serverRegistry.view<PlayerComponent>();
        for (auto pe : pView) {
            auto& pc = pView.get<PlayerComponent>(pe);
            if (pc.playerId == teamId && pc.bugClass != BugClass::None) {
                ownerClass = pc.bugClass;
                break;
            }
        }
    }
    auto data = GetTribeBuildingData(ownerClass);

    // Base HP by type – scaled by tribe bonuses
    float hp = 500.f;
    switch (type) {
        case BuildingType::Main:       hp = 500.f; break;
        case BuildingType::Storage:    hp = 350.f; break;
        case BuildingType::Barracks:   hp = 300.f; break;
        case BuildingType::Upgrade:    hp = 250.f; break;
        case BuildingType::Conversion: hp = 200.f; break;
        case BuildingType::Defense:    hp = 800.f * data.defenseHpMul; break;
        case BuildingType::Attack:     hp = 300.f; break;
        case BuildingType::Outpost:    hp = 200.f; break;
        case BuildingType::Special: {
            auto sinfo = GetSpecialBuildingInfo(specialType);
            hp = sinfo.baseHp;
            break;
        }
    }

    const uint32_t netId = m_NextNetId++;
    if (type == BuildingType::Main)
        m_TeamsWithMainBuildings.insert(teamId);

    auto e = ctx.serverRegistry.create();
    ctx.serverRegistry.emplace<TransformComponent>(e, pos);
    ctx.serverRegistry.emplace<NetworkedComponent>(e, netId);
    ctx.serverRegistry.emplace<ModelComponent>(e, model);
    ctx.serverRegistry.emplace<BuildingComponent>(e,
        BuildingComponent{type, teamId, tier, specialType, hp, hp, false});
    // Apply the owner's bug class
    auto& bc = ctx.serverRegistry.get<BuildingComponent>(e);
    bc.ownerClass = ownerClass;

    // Attach type-specific components
    switch (type) {
        case BuildingType::Main:
        case BuildingType::Storage: {
            auto& inv = ctx.serverRegistry.emplace<ResourceInventory>(e);
            // Tag the Main (and Storage) as a deposit destination for collectors.
            ctx.serverRegistry.emplace<BaseComponent>(e, BaseComponent{teamId});
            if (type == BuildingType::Main) {
                // Starting stockpile: 20 Holz + 20 of the team's signature resource.
                // 20 Holz lets the player upgrade their base once before chopping;
                // 20 signature lets them buy 4 extra workers right away (cost 5 each).
                inv.holz = 20;
                inv.Add(GetSignatureResource(ownerClass), 20);
            }
            if (type == BuildingType::Storage)
                ctx.serverRegistry.emplace<StorageComponent>(e);
            break;
        }
        case BuildingType::Barracks:
            ctx.serverRegistry.emplace<BarracksComponent>(e);
            break;
        case BuildingType::Conversion: {
            // Diet-aware default conversion recipe
            auto conv = BuildingSystem::GetDefaultConversion(ownerClass);
            ctx.serverRegistry.emplace<ConversionComponent>(e, conv);
            break;
        }
        case BuildingType::Special:
            ctx.serverRegistry.emplace<SpecialBuildingComponent>(e, specialType);
            break;
        default:
            break;
    }
    m_ServerNetMap[netId] = e;

    BuildingSpawnedPacket pkt;
    pkt.netId        = netId;
    pkt.teamId       = teamId;
    pkt.buildingType = static_cast<uint8_t>(type);
    pkt.specialType  = static_cast<uint8_t>(specialType);
    pkt.tier         = tier;
    pkt.x = pos.x; pkt.y = pos.y; pkt.z = pos.z;
    pkt.hp = hp; pkt.maxHp = hp;
    ctx.network.BroadcastToAll(pkt);

    // Also create the client-side entity directly when hosting
    {
        float startY = pos.y - 0.5f;
        auto ce = ctx.clientRegistry.create();
        ctx.clientRegistry.emplace<TransformComponent>(ce, glm::vec3{pos.x, startY, pos.z});
        ctx.clientRegistry.emplace<NetworkedComponent>(ce, netId);
        ctx.clientRegistry.emplace<ModelComponent>(ce, model);
        ctx.clientRegistry.emplace<BuildingComponent>(ce,
            BuildingComponent{type, teamId, tier, specialType, hp, hp, false});
        if (type == BuildingType::Special)
            ctx.clientRegistry.emplace<SpecialBuildingComponent>(ce, specialType);
        ctx.clientRegistry.emplace<ConstructionComponent>(ce,
            ConstructionComponent{0.f, 1.0f, startY, pos.y});
        auto& hc = ctx.clientRegistry.emplace<HealthComponent>(ce, HealthComponent{hp});
        hc.hp = hp;
        m_ClientNetMap[netId] = ce;
    }

    spdlog::info("GameScene: spawned building netId={} team={} type={} tier={} hp={:.0f}",
                 netId, teamId, (int)type, tier, hp);
    return e;
}

// ---------------------------------------------------------------------------
// ApplyUpgrade
// ---------------------------------------------------------------------------

void GameScene::ApplyUpgrade(SceneContext& ctx, uint32_t teamId, UpgradePathID pathId) {
    if (!ctx.network.IsHosting()) return;
    if (!UpgradeSystem::Apply(ctx.serverRegistry, teamId, pathId)) return;

    // Broadcast to all clients
    UpgradeCompletedPacket pkt;
    pkt.pathId = pathId;
    pkt.teamId = teamId;
    ctx.network.BroadcastToAll(pkt);

    spdlog::info("GameScene: team {} completed upgrade path {}", teamId, pathId);
}

// ---------------------------------------------------------------------------
// HandleBuildingDeath
// ---------------------------------------------------------------------------

void GameScene::HandleBuildingDeath(SceneContext& ctx, entt::entity entity, uint32_t netId)
{
    if (!ctx.serverRegistry.valid(entity)) return;

    // Broadcast destruction
    BuildingDestroyedPacket pkt;
    pkt.netId = netId;
    ctx.network.BroadcastToAll(pkt);

    // Remove from server maps
    m_ServerNetMap.erase(netId);
    ctx.serverRegistry.destroy(entity);

    spdlog::info("GameScene: building netId={} destroyed", netId);
}

// ---------------------------------------------------------------------------
// EnterPlacementMode / CancelPlacement
// ---------------------------------------------------------------------------

void GameScene::EnterPlacementMode(SceneContext& ctx, BuildingType type)
{
    CancelPlacement(ctx);

    m_SelectedBuildingType = static_cast<int>(type);
    m_PlacementActive      = true;

    // Create a client-only ghost entity
    m_GhostEntity = ctx.clientRegistry.create();
    ctx.clientRegistry.emplace<TransformComponent>(m_GhostEntity, glm::vec3{0.f, 0.f, 0.f});
    ctx.clientRegistry.emplace<ModelComponent>(m_GhostEntity, std::string("assets/cube.glb"));
    ctx.clientRegistry.emplace<GhostComponent>(m_GhostEntity);

    spdlog::info("GameScene: entered placement mode for building type {}", (int)type);
}

void GameScene::CancelPlacement(SceneContext& ctx)
{
    if (m_GhostEntity != entt::null && ctx.clientRegistry.valid(m_GhostEntity)) {
        ctx.clientRegistry.destroy(m_GhostEntity);
    }
    m_GhostEntity = entt::null;
    m_PlacementActive = false;
    m_PlacementValid = false;
    m_GhostPos = glm::vec3(0.f);
    m_FadeCenter = glm::vec3(0.f);
    m_SelectedBuildingType = -1;
}

// ---------------------------------------------------------------------------
// RandomSpawnInTerritory
// ---------------------------------------------------------------------------

glm::vec3 GameScene::RandomSpawnInTerritory(SceneContext& ctx, uint32_t teamId)
{
    // Gather all territory zone centres.
    struct ZoneInfo { glm::vec3 center; float hw, hd; };
    std::vector<ZoneInfo> candidates;

    auto view = ctx.serverRegistry.view<TransformComponent, TerritoryComponent>();
    uint32_t idx = 0;
    for (auto e : view) {
        const auto& tf  = view.get<TransformComponent>(e);
        const auto& ter = view.get<TerritoryComponent>(e);
        // FFA: assign zones round-robin by teamId
        if (idx % 24 == teamId % 24)
            candidates.push_back({tf.position, ter.halfW, ter.halfD});
        idx++;
    }

    // Fallback: spread by team (FFA: shift by teamId * 20)
    if (candidates.empty()) {
        float x = -100.f + (float)teamId * 30.f;
        return {x, 0.f, 0.f};
    }

    static std::mt19937 rng{std::random_device{}()};
    const auto& zone = candidates[rng() % candidates.size()];
    std::uniform_real_distribution<float> rx(-zone.hw, zone.hw);
    std::uniform_real_distribution<float> rz(-zone.hd, zone.hd);
    return {zone.center.x + rx(rng), 0.f, zone.center.z + rz(rng)};
}

// ---------------------------------------------------------------------------
// UpdateUnitMovement  (server tick)
// ---------------------------------------------------------------------------

/**
 * @brief Moves units with an active MovementOrderComponent toward their
 *        destination.  Simple steering: normalise direction, scale by speed,
 *        stop within 1 unit.
 */
void GameScene::UpdateUnitMovement(SceneContext& ctx, float dt)
{
    // Build a per‑tile occupancy grid from buildings (2×2 blocks).
    const int gs = m_World.GetGridSize();
    std::vector<bool> occupiedTiles(static_cast<size_t>(gs) * gs, false);
    {
        auto bldView = ctx.serverRegistry.view<TransformComponent, BuildingComponent>();
        for (auto be : bldView) {
            const auto& bc = bldView.get<BuildingComponent>(be);
            if (bc.destroyed) continue;
            const auto& btf = bldView.get<TransformComponent>(be);
            int tx, tz;
            m_World.WorldToTile(btf.position.x, btf.position.z, tx, tz);
            // 2×2 building footprint.
            for (int dz = 0; dz < 2; ++dz) {
                for (int dx = 0; dx < 2; ++dx) {
                    int cx = tx + dx;
                    int cz = tz + dz;
                    if (cx >= 0 && cx < gs && cz >= 0 && cz < gs)
                        occupiedTiles[(size_t)cz * gs + (size_t)cx] = true;
                }
            }
        }
    }

    // Steering‑level terrain check (building avoidance is handled by the A* pathfinder).
    auto canStand = [&](glm::vec2 np, int currentTier, bool climber) -> bool {
        return IsWalkableAt(m_World, np.x, np.y, currentTier, false, climber);
    };

    auto view = ctx.serverRegistry.view<TransformComponent, MovementComponent,
                                        MovementOrderComponent, UnitComponent>();
    for (auto e : view) {
        auto& tf = view.get<TransformComponent>(e);
        auto& mv = view.get<MovementComponent>(e);
        auto& mo = view.get<MovementOrderComponent>(e);
        auto& uc = view.get<UnitComponent>(e);
        bool isFlying  = IsFlying(uc.bugClass);
        bool isClimber = IsClimber(uc.bugClass);

        // Combat target pursuit: if this unit has a commanded attack target
        // that is out of range, keep moving toward it each tick.
        auto* cc = ctx.serverRegistry.try_get<CombatComponent>(e);
        if (cc && cc->commandedTarget && cc->target != entt::null &&
            ctx.serverRegistry.valid(cc->target)) {
            auto* ttf = ctx.serverRegistry.try_get<TransformComponent>(cc->target);
            if (ttf) {
                float distToTarget = glm::length(ttf->position - tf.position);
                if (distToTarget > cc->attackRange + 0.5f) {
                    // Out of range: chase.
                    mo.active = true;
                    if (glm::length(mo.destination - ttf->position) > 0.5f) {
                        mo.destination = ttf->position;
                        // Mark path dirty so it recomputes next tick.
                        auto& path = ctx.serverRegistry.get_or_emplace<PathComponent>(e);
                        path.dirty = true;
                    }
                } else {
                    // In attack range — stop moving; UpdateCombat handles firing.
                    mo.active = false;
                    mv.velocity = {0.f, 0.f, 0.f};
                    continue;
                }
            } else {
                // Target lost its transform — invalidate.
                cc->target = entt::null;
                cc->commandedTarget = false;
            }
        }

        if (!mo.active) {
            mv.velocity = {0.f, 0.f, 0.f};
            continue;
        }

        // Ensure a PathComponent exists.
        auto& path = ctx.serverRegistry.get_or_emplace<PathComponent>(e);

        // When a new order arrives (destination changed), compute a fresh path.
        if (path.waypoints.empty() || path.dirty) {
            // Use the unit's team fog grid for pathfinding
            auto fit = m_TeamFogs.find(uc.teamId);
            const FogGrid* fog = (fit != m_TeamFogs.end() && fit->second.IsInitialised())
                                 ? &fit->second : nullptr;
            path.waypoints = Pathfinding::FindPath(
                m_World,
                glm::vec2(tf.position.x, tf.position.z),
                glm::vec2(mo.destination.x, mo.destination.z),
                fog,
                1,
                &occupiedTiles,
                isFlying,
                isClimber);
            path.current   = 0;
            path.dirty     = false;
            path.recalcTimer = 0.f;

            if (path.waypoints.empty()) {
                spdlog::debug("Pathfinding: no path for entity {}, stopping", (uint32_t)e);
                mo.active   = false;
                mv.velocity = {0.f, 0.f, 0.f};
                continue;
            }
        }

        // Periodic recalculation so the path adapts to newly discovered fog
        // tiles or changes in building placement.
        path.recalcTimer += dt;
        if (path.recalcTimer >= PathComponent::RECALC_INTERVAL) {
            auto fit = m_TeamFogs.find(uc.teamId);
            const FogGrid* fog = (fit != m_TeamFogs.end() && fit->second.IsInitialised())
                                 ? &fit->second : nullptr;
            path.waypoints = Pathfinding::FindPath(
                m_World,
                glm::vec2(tf.position.x, tf.position.z),
                glm::vec2(mo.destination.x, mo.destination.z),
                fog,
                1,
                &occupiedTiles,
                isFlying,
                isClimber);
            path.current   = 0;
            path.recalcTimer = 0.f;
            if (path.waypoints.empty()) {
                mo.active   = false;
                mv.velocity = {0.f, 0.f, 0.f};
                continue;
            }
        }

        // Advance to the next waypoint if we're close enough.
        glm::vec2 targetWp = path.waypoints[path.current];
        glm::vec2 curXZ    = glm::vec2(tf.position.x, tf.position.z);
        glm::vec2 toWp     = targetWp - curXZ;
        float distToWp     = glm::length(toWp);

        if (distToWp < 0.5f) {
            ++path.current;
            if (path.current >= (int)path.waypoints.size()) {
                // All waypoints reached — order complete.
                mo.active   = false;
                mv.velocity = {0.f, 0.f, 0.f};
                if (isFlying) {
                    float targetY = GroundHeightAt(m_World, tf.position.x, tf.position.z) + 4.f;
                    tf.position.y = glm::mix(tf.position.y, targetY, 10.f * dt);
                } else
                    tf.position.y = GroundHeightAt(m_World, tf.position.x, tf.position.z);
                path.waypoints.clear();
                continue;
            }
            targetWp = path.waypoints[path.current];
            toWp     = targetWp - curXZ;
            distToWp = glm::length(toWp);
        }

        glm::vec2 dir = distToWp > 0.001f ? toWp / distToWp : glm::vec2(0.f);
        glm::vec2 step = dir * (mv.speed * dt);

        if (isFlying) {
            // Flying units move freely — no terrain sliding, no ground snap.
            tf.position.x += step.x;
            tf.position.z += step.y;
            float targetY = GroundHeightAt(m_World, tf.position.x, tf.position.z) + 4.f;
            tf.position.y  = glm::mix(tf.position.y, targetY, 10.f * dt);
            mv.velocity    = glm::vec3(step.x, 0.f, step.y) / dt;
        } else {
            int curTx, curTz;
            m_World.WorldToTile(curXZ.x, curXZ.y, curTx, curTz);
            int currentTier = (int)m_World.GetTile(curTx, curTz).tier;

            // Slide along X / Z if the full step is blocked.
            glm::vec2 chosen{0.f};
            if      (canStand(curXZ + step,                       currentTier, isClimber)) chosen = step;
            else if (canStand(curXZ + glm::vec2(step.x, 0.f),     currentTier, isClimber)) chosen = {step.x, 0.f};
            else if (canStand(curXZ + glm::vec2(0.f,    step.y),  currentTier, isClimber)) chosen = {0.f,    step.y};
            else                                                                           chosen = {0.f, 0.f};

            if (chosen.x == 0.f && chosen.y == 0.f) {
                mv.velocity = {0.f, 0.f, 0.f};
                continue;
            }

            tf.position.x += chosen.x;
            tf.position.z += chosen.y;
            tf.position.y  = GroundHeightAt(m_World, tf.position.x, tf.position.z);
            mv.velocity    = glm::vec3(chosen.x, 0.f, chosen.y) / dt;
        }

        if (isFlying) {
            // Face the final destination so flying units don't jitter between
            // stair-stepped waypoints (the A* pathfinder only uses 4 cardinal
            // directions).
            glm::vec2 toDest = glm::vec2(mo.destination.x, mo.destination.z) - curXZ;
            float distToDest = glm::length(toDest);
            if (distToDest > 0.001f) {
                glm::vec2 destDir = toDest / distToDest;
                tf.rotation.y = glm::degrees(std::atan2(destDir.x, destDir.y));
            }
        } else if (distToWp > 0.001f) {
            tf.rotation.y = glm::degrees(std::atan2(dir.x, dir.y));
        }
    }
}

// ---------------------------------------------------------------------------
// UpdateCombat  (server tick)
// ---------------------------------------------------------------------------

/**
 * @brief Auto-combat system.
 *
 * For each unit with a CombatComponent:
 *  1. If current target is invalid or dead, search for nearest enemy unit
 *     within attackRange.
 *  2. If target found and in range, tick cooldown and deal damage when it
 *     fires.  Also pause movement while attacking.
 *  3. Dead units (hp <= 0) get HandleUnitDeath called.
 */
void GameScene::UpdateCombat(SceneContext& ctx, float dt)
{
    // Collect all units with health for target queries
    struct UnitInfo { entt::entity e; glm::vec3 pos; uint32_t team; float hp; };
    std::vector<UnitInfo> unitInfos;
    {
        auto view = ctx.serverRegistry.view<TransformComponent, UnitComponent, HealthComponent>();
        for (auto e : view) {
            auto& tc = view.get<TransformComponent>(e);
            auto& uc = view.get<UnitComponent>(e);
            auto& hc = view.get<HealthComponent>(e);
            if (!hc.dead)
                unitInfos.push_back({e, tc.position, uc.teamId, hc.hp});
        }
    }

    // Tick combat for every unit that has a CombatComponent
    auto combatView = ctx.serverRegistry.view<TransformComponent, UnitComponent,
                                               HealthComponent, CombatComponent,
                                               MovementOrderComponent>();
    struct KillRec { entt::entity dead; uint32_t netId; uint32_t killerTeam; };
    std::vector<KillRec> toKill;

    for (auto e : combatView) {
        auto& tf  = combatView.get<TransformComponent>(e);
        auto& uc  = combatView.get<UnitComponent>(e);
        auto& hc  = combatView.get<HealthComponent>(e);
        auto& cc  = combatView.get<CombatComponent>(e);
        auto& mo  = combatView.get<MovementOrderComponent>(e);

        if (hc.dead) continue;

        // Tick attack cooldown
        if (cc.attackCooldown > 0.f) cc.attackCooldown -= dt;

        // Validate current target
        bool targetValid = (cc.target != entt::null) &&
                           ctx.serverRegistry.valid(cc.target);
        if (targetValid) {
            auto* thc = ctx.serverRegistry.try_get<HealthComponent>(cc.target);
            if (!thc || thc->dead) {
                targetValid = false;
                cc.target = entt::null;
                cc.commandedTarget = false;
            }
        }

        // Find nearest enemy if no valid target (only for auto-acquire, not commanded)
        if (!targetValid && !cc.commandedTarget) {
            float bestDist = cc.attackRange;
            for (const auto& info : unitInfos) {
                if (info.team == uc.teamId) continue;
                float d = glm::length(info.pos - tf.position);
                if (d < bestDist) { bestDist = d; cc.target = info.e; targetValid = true; }
            }
        }

        if (!targetValid) continue;

        // Check range
        auto* ttf = ctx.serverRegistry.try_get<TransformComponent>(cc.target);
        if (!ttf) { cc.target = entt::null; cc.commandedTarget = false; continue; }
        float dist = glm::length(ttf->position - tf.position);

        if (dist <= cc.attackRange) {
            // Pause movement while in combat
            mo.active = false;

            if (cc.attackCooldown <= 0.f) {
                cc.attackCooldown = cc.attackRate;

                auto* thc = ctx.serverRegistry.try_get<HealthComponent>(cc.target);
                if (thc && !thc->dead) {
                    thc->hp -= cc.attackDamage;
                    spdlog::debug("Combat: unit {} hits {} for {:.0f} dmg (hp={:.0f})",
                                  (uint32_t)e, (uint32_t)cc.target, cc.attackDamage, thc->hp);

                    // Broadcast HP update
                    auto* tnc = ctx.serverRegistry.try_get<NetworkedComponent>(cc.target);
                    if (tnc) {
                        UnitHpUpdatePacket hp_pkt;
                        hp_pkt.netId = tnc->netId;
                        hp_pkt.hp    = thc->hp;
                        ctx.network.BroadcastToAll(hp_pkt);
                    }

                    if (thc->hp <= 0.f) {
                        thc->dead = true;
                        auto* dnc = ctx.serverRegistry.try_get<NetworkedComponent>(cc.target);
                        uint32_t dnetId = dnc ? dnc->netId : 0;
                        toKill.push_back({cc.target, dnetId, uc.teamId});
                        cc.target = entt::null;
                    }
                }
            }
        }
    }

    // Also check building health – units attack enemy buildings if no other target
    {
        auto bldView = ctx.serverRegistry.view<TransformComponent, BuildingComponent>();
        for (auto& info : unitInfos) {
            auto* cc2 = ctx.serverRegistry.try_get<CombatComponent>(info.e);
            if (!cc2 || cc2->target != entt::null) continue;

            float bestDist = cc2->attackRange;
            entt::entity bestBld = entt::null;
            for (auto be : bldView) {
                auto& btf = bldView.get<TransformComponent>(be);
                auto& bc  = bldView.get<BuildingComponent>(be);
                if (bc.teamId == info.team || bc.destroyed) continue;
                float d = glm::length(btf.position - info.pos);
                if (d < bestDist) { bestDist = d; bestBld = be; }
            }
            if (bestBld == entt::null) continue;

            if (cc2->attackCooldown <= 0.f) {
                cc2->attackCooldown = cc2->attackRate;
                auto& bc = bldView.get<BuildingComponent>(bestBld);
                bc.hp -= cc2->attackDamage;
                spdlog::debug("Combat: unit {} hits building {} for {:.0f} dmg (hp={:.0f})",
                              (uint32_t)info.e, (uint32_t)bestBld, cc2->attackDamage, bc.hp);

                // Broadcast building HP update
                auto* bnc = ctx.serverRegistry.try_get<NetworkedComponent>(bestBld);
                if (bnc) {
                    UnitHpUpdatePacket hp_pkt;
                    hp_pkt.netId = bnc->netId;
                    hp_pkt.hp    = bc.hp;
                    ctx.network.BroadcastToAll(hp_pkt);
                }

                if (bc.hp <= 0.f) {
                    bc.hp = 0.f;
                    bc.destroyed = true;
                    uint32_t bnetId = bnc ? bnc->netId : 0;
                    HandleBuildingDeath(ctx, bestBld, bnetId);
                }
            }
        }
    }

    // Process kills
    for (auto& kr : toKill) {
        UpgradeSystem::AddKill(kr.killerTeam);
        HandleUnitDeath(ctx, kr.dead, kr.netId);
    }
}

// ---------------------------------------------------------------------------
// HandleUnitDeath
// ---------------------------------------------------------------------------

void GameScene::HandleUnitDeath(SceneContext& ctx, entt::entity entity, uint32_t netId)
{
    if (!ctx.serverRegistry.valid(entity)) return;

    auto* tf = ctx.serverRegistry.try_get<TransformComponent>(entity);
    if (tf) {
        // Drop a Fleisch resource at death position
        m_ResourceManager.SpawnMeatDrop(ctx.serverRegistry, tf->position,
                                        /*amount=*/1, /*lifetime=*/20.f);
    }

    // Broadcast death
    UnitDiedPacket pkt;
    pkt.netId = netId;
    ctx.network.BroadcastToAll(pkt);

    // Remove from server maps
    m_ServerNetMap.erase(netId);
    ctx.serverRegistry.destroy(entity);

    spdlog::info("GameScene: unit {} died, Fleisch dropped", netId);
}

// ---------------------------------------------------------------------------
// CheckWinCondition
// ---------------------------------------------------------------------------

/**
 * @brief Win condition: a team wins when ALL enemy Main buildings are destroyed.
 *
 * Checks BuildingComponent entities with BuildingType::Main.
 * Falls back to legacy BaseHealthComponent if no Main buildings exist.
 */
void GameScene::CheckWinCondition(SceneContext& ctx)
{
    if (m_GameOver) return;

    // Count surviving Main buildings per team
    std::unordered_map<uint32_t, int> surviving;
    {
        auto view = ctx.serverRegistry.view<BuildingComponent>();
        for (auto e : view) {
            const auto& bc = view.get<BuildingComponent>(e);
            if (bc.type == BuildingType::Main && !bc.destroyed)
                surviving[bc.teamId]++;
        }
    }

    // Fallback: legacy BaseHealthComponent
    if (m_TeamsWithMainBuildings.empty()) {
        auto view = ctx.serverRegistry.view<BaseHealthComponent>();
        for (auto e : view) {
            const auto& bhc = view.get<BaseHealthComponent>(e);
            if (!bhc.destroyed) surviving[bhc.teamId]++;
        }
        if (surviving.empty()) return;

        std::vector<uint32_t> eliminated;
        for (const auto& [team, cnt] : surviving)
            if (cnt == 0) eliminated.push_back(team);
        if (eliminated.empty()) return;

        uint32_t winner = 0xFFFFFFFFu;
        for (const auto& [team, cnt] : surviving)
            if (cnt > 0) { winner = team; break; }

        m_GameOver   = true;
        m_GameOverTimer = 0.f;
        m_WinnerTeam = winner;

        GameOverPacket gopkt;
        gopkt.winnerTeam = winner;
        ctx.network.BroadcastToAll(gopkt);
        spdlog::info("GameScene: WIN CONDITION – team {} wins", winner);
        return;
    }

    // Normal path: teams tracked by m_TeamsWithMainBuildings
    // Find eliminated teams (those in the set but with no surviving Main)
    std::vector<uint32_t> eliminated;
    for (uint32_t team : m_TeamsWithMainBuildings) {
        if (surviving[team] == 0)
            eliminated.push_back(team);
    }

    if (eliminated.empty()) return;

    // Survivors (teams that still have at least one Main building)
    std::vector<uint32_t> survivors;
    for (uint32_t team : m_TeamsWithMainBuildings) {
        if (surviving[team] > 0)
            survivors.push_back(team);
    }

    // Game ends when only one team remains with Main buildings
    uint32_t winner = 0xFFFFFFFFu;
    if (survivors.size() == 1) {
        winner = survivors[0];
    } else if (survivors.empty() && eliminated.size() >= 2) {
        // All teams eliminated simultaneously (mutual destruction)
        winner = 0xFFFFFFFFu; // draw
    } else {
        return; // More than one survivor — game continues
    }

    m_GameOver   = true;
    m_GameOverTimer = 0.f;
    m_WinnerTeam = winner;

    GameOverPacket gopkt;
    gopkt.winnerTeam = winner;
    ctx.network.BroadcastToAll(gopkt);
    spdlog::info("GameScene: WIN CONDITION – team {} wins", winner);
}

// ---------------------------------------------------------------------------
// ScreenToWorldXZ  (Commander mode unprojection)
// ---------------------------------------------------------------------------

/**
 * @brief Unprojects a screen-space position onto the Y=0 XZ plane.
 *
 * Uses the camera's stored view+projection matrices and a ray-plane
 * intersection.  Returns a rough world position usable as a move target.
 */
glm::vec3 GameScene::ScreenToWorldXZ(float sx, float sy, int winW, int winH)
{
    if (!m_Camera) return {0.f, 0.f, 0.f};
    // NDC
    float ndcX = (2.f * sx / (float)winW) - 1.f;
    float ndcY = 1.f - (2.f * sy / (float)winH);

    float aspect = (winW > 0) ? (float)winW / (float)winH : 1.f;
    glm::mat4 proj = m_Camera->GetProjectionMatrix(aspect);
    glm::mat4 view = m_Camera->GetViewMatrix();
    glm::mat4 invVP = glm::inverse(proj * view);

    glm::vec4 nearPt = invVP * glm::vec4(ndcX, ndcY, -1.f, 1.f);
    glm::vec4 farPt  = invVP * glm::vec4(ndcX, ndcY,  1.f, 1.f);
    nearPt /= nearPt.w;
    farPt  /= farPt.w;

    glm::vec3 rayOrig{nearPt};
    glm::vec3 rayDir = glm::normalize(glm::vec3(farPt) - rayOrig);

    if (std::abs(rayDir.y) < 1e-6f) return {0.f, 0.f, 0.f};

    // First intersect with Y=0 to get an initial XZ estimate
    float t = -rayOrig.y / rayDir.y;
    glm::vec3 hit = rayOrig + t * rayDir;

    // Re-intersect with the actual terrain height at the estimated XZ, so the
    // cursor lands on the correct tile even when the terrain sits above Y=0
    // (important for orthographic isometric mode where parallel rays magnify
    // the parallax error between the Y=0 guess and the real ground height).
    float groundY = GroundHeightAt(m_World, hit.x, hit.z);
    t = (groundY - rayOrig.y) / rayDir.y;
    return rayOrig + t * rayDir;
}

// ---------------------------------------------------------------------------
// DrawUnitHPBars  (Commander mode UI overlay)
// ---------------------------------------------------------------------------

/**
 * @brief Draws HP bars above each unit in Commander mode.
 *
 * Projects each unit's 3D position to screen space and draws a coloured
 * progress bar via ImGui's background draw list.
 */
void GameScene::DrawUnitHPBars(SceneContext& ctx)
{
    if (!m_Camera) return;
    int winW, winH;
    SDL_GetWindowSizeInPixels(ctx.renderer->GetWindow()->handle, &winW, &winH);
    if (winW <= 0 || winH <= 0) return;

    float aspect = (float)winW / (float)winH;
    glm::mat4 vp = m_Camera->GetProjectionMatrix(aspect) * m_Camera->GetViewMatrix();

    // Fog check: hide HP bars for entities not revealed to the local player
    const FogGrid* fog = nullptr;
    {
        auto fit = m_TeamFogs.find(m_MyPlayerId);
        if (fit != m_TeamFogs.end() && fit->second.IsInitialised())
            fog = &fit->second;
    }

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    constexpr float BAR_W = 40.f, BAR_H = 5.f;

    // Units
    {
        auto view = ctx.clientRegistry.view<TransformComponent, HealthComponent, UnitComponent, NetworkedComponent>();
        for (auto e : view) {
            const auto& tf  = view.get<TransformComponent>(e);
            const auto& hc  = view.get<HealthComponent>(e);
            const auto& uc  = view.get<UnitComponent>(e);
            const auto& nc  = view.get<NetworkedComponent>(e);

            if (fog && !fog->IsWorldPosRevealed(tf.position)) continue;

            glm::vec4 clip = vp * glm::vec4(tf.position + glm::vec3(0, 2.5f, 0), 1.f);
            if (clip.w <= 0.f) continue;
            clip /= clip.w;
            if (clip.x < -1.f || clip.x > 1.f || clip.y < -1.f || clip.y > 1.f) continue;

            float sx = (clip.x * 0.5f + 0.5f) * (float)winW;
            float sy = (1.f - (clip.y * 0.5f + 0.5f)) * (float)winH;

            ImVec2 bmin{sx - BAR_W * 0.5f, sy};
            ImVec2 bmax{sx + BAR_W * 0.5f, sy + BAR_H};
            dl->AddRectFilled(bmin, bmax, IM_COL32(30, 30, 30, 200));

            float frac = (hc.maxHp > 0.f) ? glm::clamp(hc.hp / hc.maxHp, 0.f, 1.f) : 0.f;
            ImU32 col = (uc.teamId == 0)
                ? IM_COL32(60, 140, 255, 220)
                : IM_COL32(255, 60, 60, 220);
            dl->AddRectFilled(bmin, ImVec2(bmin.x + BAR_W * frac, bmax.y), col);

            bool selected = std::find(m_SelectedUnits.begin(), m_SelectedUnits.end(), nc.netId)
                            != m_SelectedUnits.end();
            if (selected)
                dl->AddRect(ImVec2(bmin.x - 1, bmin.y - 1), ImVec2(bmax.x + 1, bmax.y + 1),
                            IM_COL32(255, 255, 0, 255), 0.f, 0, 1.5f);
        }
    }

    // Buildings (wider bars, different colour tint)
    {
        auto view = ctx.clientRegistry.view<TransformComponent, HealthComponent, BuildingComponent, NetworkedComponent>();
        for (auto e : view) {
            const auto& tf  = view.get<TransformComponent>(e);
            const auto& hc  = view.get<HealthComponent>(e);
            const auto& bc  = view.get<BuildingComponent>(e);
            const auto& nc  = view.get<NetworkedComponent>(e);
            if (bc.destroyed) continue;

            if (fog && !fog->IsWorldPosRevealed(tf.position)) continue;

            constexpr float BLD_BAR_W = 50.f, BLD_BAR_H = 6.f;
            glm::vec4 clip = vp * glm::vec4(tf.position + glm::vec3(0, 3.5f, 0), 1.f);
            if (clip.w <= 0.f) continue;
            clip /= clip.w;
            if (clip.x < -1.f || clip.x > 1.f || clip.y < -1.f || clip.y > 1.f) continue;

            float sx = (clip.x * 0.5f + 0.5f) * (float)winW;
            float sy = (1.f - (clip.y * 0.5f + 0.5f)) * (float)winH;

            ImVec2 bmin{sx - BLD_BAR_W * 0.5f, sy};
            ImVec2 bmax{sx + BLD_BAR_W * 0.5f, sy + BLD_BAR_H};
            dl->AddRectFilled(bmin, bmax, IM_COL32(20, 20, 20, 200));

            float frac = (hc.maxHp > 0.f) ? glm::clamp(hc.hp / hc.maxHp, 0.f, 1.f) : 0.f;
            // Team colour with a golden tint
            ImU32 col = (bc.teamId == 0)
                ? IM_COL32(100, 180, 255, 230)
                : IM_COL32(255, 100, 100, 230);
            dl->AddRectFilled(bmin, ImVec2(bmin.x + BLD_BAR_W * frac, bmax.y), col);

            // Construction indicator
            bool underConstruction = ctx.clientRegistry.any_of<ConstructionComponent>(e);
            if (underConstruction) {
                // Show "IN BAU" label below the bar
                ImVec2 labelPos{sx - 20.f, sy + BLD_BAR_H + 1.f};
                dl->AddText(labelPos, IM_COL32(255, 200, 0, 255), "IN BAU");
            } else {
                // Show green checkmark for completed buildings
                ImVec2 labelPos{sx - 5.f, sy + BLD_BAR_H + 1.f};
                dl->AddText(labelPos, IM_COL32(0, 220, 0, 255), "OK");
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Territory snapshot broadcast  (server)
// ---------------------------------------------------------------------------

void GameScene::SendTerritorySnapshot(SceneContext& ctx)
{
    if (!ctx.network.IsHosting()) return;

    TerritorySnapshotPacket pkt;
    auto view = ctx.serverRegistry.view<TransformComponent, TerritoryComponent>();
    for (auto e : view) {
        if (pkt.zoneCount >= 8) break;
        const auto& tf  = view.get<TransformComponent>(e);
        const auto& ter = view.get<TerritoryComponent>(e);
        auto& zd = pkt.zones[pkt.zoneCount++];
        std::strncpy(zd.name, ter.name, sizeof(zd.name) - 1);
        zd.halfW           = ter.halfW;
        zd.halfD           = ter.halfD;
        zd.captureTime     = ter.captureTime;
        zd.captureProgress = ter.captureProgress;
        zd.ownerTeam       = ter.ownerTeam;
        zd.contestedBy     = ter.contestedBy;
        zd.centerX         = tf.position.x;
        zd.centerY         = tf.position.y;
        zd.centerZ         = tf.position.z;
    }
    ctx.network.BroadcastToAll(pkt);
}

// ---------------------------------------------------------------------------
// Fog snapshot broadcast  (server)
// ---------------------------------------------------------------------------

void GameScene::SendFogSnapshot(SceneContext& ctx)
{
    if (!ctx.network.IsHosting()) return;

    // Build a reverse-map: netId → peerId
    std::unordered_map<uint32_t, uint32_t> netIdToPeer;
    for (auto& [pid, nid] : m_PeerToNetId)
        netIdToPeer[nid] = pid;

    // Send per-team fog deltas to each connected player
    for (auto& [pid, nid] : m_PeerToNetId) {
        uint32_t peerId = pid;
        uint32_t playerNetId = nid;

        auto eit = m_ServerNetMap.find(playerNetId);
        if (eit == m_ServerNetMap.end()) continue;
        auto* pc = ctx.serverRegistry.try_get<PlayerComponent>(eit->second);
        if (!pc) continue;

        uint32_t teamId = pc->playerId; // FFA: playerId == teamId

        auto fit = m_TeamFogs.find(teamId);
        if (fit == m_TeamFogs.end() || !fit->second.IsInitialised()) continue;

        // Drain dirty cells into a delta packet
        std::vector<uint32_t> dirtyCells;
        fit->second.ConsumeDirty(dirtyCells);
        if (dirtyCells.empty()) continue;

        // Send in batches of 512 cells
        size_t sent = 0;
        while (sent < dirtyCells.size()) {
            FogDeltaPacket dpkt;
            size_t chunk = std::min<size_t>(512, dirtyCells.size() - sent);
            dpkt.count = static_cast<uint16_t>(chunk);
            for (size_t i = 0; i < chunk; ++i)
                dpkt.cells[i] = dirtyCells[sent + i];
            ctx.network.SendToClient(peerId, dpkt);
            sent += chunk;
        }
    }
}

// ---------------------------------------------------------------------------
// Per-base inventory broadcast (server, 2 Hz).
// Iterates every server entity that has BOTH BaseComponent and ResourceInventory
// (currently the MainBase + any Storage building), packs the stockpile into an
// InventoryUpdatePacket, broadcasts to every client, AND writes the same data
// to the host's own m_ClientBaseInventories mirror — so the HUD reads from one
// place regardless of host/client.
// ---------------------------------------------------------------------------
void GameScene::SendInventoryUpdates(SceneContext& ctx)
{
    if (!ctx.network.IsHosting()) return;

    auto view = ctx.serverRegistry.view<BaseComponent, ResourceInventory, NetworkedComponent>();
    for (auto e : view) {
        const auto& bc  = view.get<BaseComponent>(e);
        const auto& inv = view.get<ResourceInventory>(e);
        const auto& nc  = view.get<NetworkedComponent>(e);

        InventoryUpdatePacket pkt;
        pkt.baseNetId = nc.netId;
        pkt.teamId    = bc.teamId;
        pkt.pilze     = inv.pilze;
        pkt.beeren    = inv.beeren;
        pkt.nektar    = inv.nektar;
        pkt.samen     = inv.samen;
        pkt.insekten  = inv.insekten;
        pkt.fleisch   = inv.fleisch;
        pkt.holz      = inv.holz;
        ctx.network.BroadcastToAll(pkt);

        // Host-local mirror — BroadcastToAll doesn't loop back to host's client.
        ResourceInventory& mirror = m_ClientBaseInventories[nc.netId];
        mirror = inv;
    }
}

// ---------------------------------------------------------------------------
// Resource sync  (server)
// ---------------------------------------------------------------------------

void GameScene::SyncResourceSpawns(SceneContext& ctx)
{
    if (!ctx.network.IsHosting()) return;

    // Assign netIds to newly spawned resources (e.g. meat drops, respawned nodes)
    auto view = ctx.serverRegistry.view<TransformComponent, ResourceComponent>();
    for (auto entity : view) {
        if (ctx.serverRegistry.try_get<NetworkedComponent>(entity))
            continue;

        uint32_t netId = m_NextNetId++;
        ctx.serverRegistry.emplace<NetworkedComponent>(entity, netId);
        m_ResourceNetMap[netId] = entity;
        m_ServerNetMap[netId]   = entity; // also tracked for snapshots

        auto& res = view.get<ResourceComponent>(entity);
        auto& tf  = view.get<TransformComponent>(entity);

        ResourceSpawnedPacket pkt;
        pkt.netId = netId;
        pkt.resourceType = static_cast<uint8_t>(res.type);
        pkt.x = tf.position.x;
        pkt.y = tf.position.y;
        pkt.z = tf.position.z;
        ctx.network.BroadcastToAll(pkt);

        spdlog::debug("Resource synced: netId={} type={} at ({:.1f},{:.1f},{:.1f})",
                      netId, (int)res.type, tf.position.x, tf.position.y, tf.position.z);
    }

    // Detect depleted or destroyed resources and broadcast removal
    std::vector<uint32_t> toRemove;
    for (auto& [netId, entity] : m_ResourceNetMap) {
        if (!ctx.serverRegistry.valid(entity)) {
            toRemove.push_back(netId);
            continue;
        }
        auto* res = ctx.serverRegistry.try_get<ResourceComponent>(entity);
        if (res && res->depleted) {
            toRemove.push_back(netId);
        }
    }
    for (uint32_t netId : toRemove) {
        ResourceDepletedPacket pkt;
        pkt.netId = netId;
        ctx.network.BroadcastToAll(pkt);
        m_ResourceNetMap.erase(netId);
    }
}

// ---------------------------------------------------------------------------
// Client-side handlers
// ---------------------------------------------------------------------------

void GameScene::HandleTerritorySnapshot(const TerritorySnapshotPacket& pkt)
{
    m_ClientTerritories.clear();
    uint32_t count = std::min(pkt.zoneCount, 8u);
    for (uint32_t i = 0; i < count; ++i) {
        const auto& src = pkt.zones[i];
        ClientTerritoryZone cz;
        std::strncpy(cz.name, src.name, sizeof(cz.name) - 1);
        cz.halfW           = src.halfW;
        cz.halfD           = src.halfD;
        cz.captureProgress = src.captureProgress;
        cz.captureTime     = src.captureTime;
        cz.ownerTeam       = src.ownerTeam;
        cz.contestedBy     = src.contestedBy;
        cz.center          = glm::vec3{src.centerX, src.centerY, src.centerZ};
        m_ClientTerritories.push_back(cz);
    }
}

void GameScene::HandleFogSnapshot(const FogSnapshotPacket& pkt)
{
    if (pkt.cellsX == 0 || pkt.cellsZ == 0) return;

    // Initialise client fog grid with extents matching the server
    m_ClientFog.Init(
        glm::vec3{-375.f, 0.f, -375.f},
        glm::vec3{ 375.f, 0.f,  375.f},
        /*cellSize=*/2.f
    );

    size_t totalCells = static_cast<size_t>(pkt.cellsX) * static_cast<size_t>(pkt.cellsZ);
    size_t words = (totalCells + 63) / 64;
    if (words > 2200) words = 2200;

    for (size_t w = 0; w < words; ++w) {
        for (size_t b = 0; b < 64; ++b) {
            size_t idx = w * 64 + b;
            if (idx >= totalCells) break;
            if (pkt.gridData[w] & (uint64_t(1) << b)) {
                int z = static_cast<int>(idx / pkt.cellsX);
                int x = static_cast<int>(idx % pkt.cellsX);
                m_ClientFog.revealed[idx] = true;
            }
        }
    }
    m_HasFogData = true;
}


