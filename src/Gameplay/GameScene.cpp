/**
 * @file GameScene.cpp
 * @brief Implementation of the main gameplay scene.
 */

#define GLM_ENABLE_EXPERIMENTAL
#include "GameScene.h"
#include "MainMenuScene.h"
#include "World.h"
#include "Components.h"
#include "Systems.h"
#include "../Networking/NetworkManager.h"
#include "../Networking/Packets.h"
#include "../Core/Input.h"
#include "../Core/AssetManager.h"
#include "../Core/MeshCollisionBuilder.h"
#include "../Graphics/Renderer.h"
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
// Lifecycle
// ---------------------------------------------------------------------------

GameScene::GameScene() {}
GameScene::~GameScene() {}

/**
 * @brief Initializes the camera and spawns a test asset if hosting.
 * @param ctx The scene context.
 */
void GameScene::OnEnter(SceneContext& ctx) {
    spdlog::info("GameScene: entered");
    UpgradeSystem::Reset();

    if (!m_Camera) {
        m_Camera = std::make_unique<Camera>();
        m_Camera->m_Position = {0, 2, 10};
        m_Camera->m_Yaw = -90.0f;
        m_Camera->m_Pitch = 0.0f;
        m_Camera->UpdateVectors();
    }

    if (ctx.network.IsHosting()) {
        /**
         * @brief Load the scene's mesh geometry as static physics collision.
         *
         * This replaces the flat AddStaticFloor() placeholder with accurate
         * per-triangle collision derived from the actual GLTF scene mesh.
         * The same asset path used for visual rendering is reused here —
         * AssetManager returns cached CPU-side vertices/indices at no extra
         * file-IO cost.
         */
        LoadSceneMeshCollision(ctx, "assets/test_scene_pixelation.glb");

        // Initialize resource system
        m_ResourceManager.Init();
        m_ResourceManager.SpawnPermanentResources(ctx.serverRegistry);

        // Demo: spawn a Fleisch drop that disappears after 15 s
        m_ResourceManager.SpawnMeatDrop(ctx.serverRegistry,
                                        glm::vec3{0.f, 0.f, 3.f},
                                        /*amount=*/2,
                                        /*dropLifetime=*/15.f);

        // Assign netIds to all permanent resources and broadcast to clients
        SyncResourceSpawns(ctx);

        // Fog of War – initialise grid to match the map extents
        m_Fog.Init(glm::vec3{-50.f, 0.f, -50.f},
                   glm::vec3{ 50.f, 0.f,  50.f},
                   /*cellSize=*/2.f);

        // Territory zones
        TerritorySystem::SpawnZones(ctx.serverRegistry);

        // HUD-Texturen laden (Pixel-Art-Icons des HUD-Designers)
        HUDTextures::Load(ctx.renderer->GetDevice());

        // -----------------------------------------------------------------
        // Spawn team buildings (destroyable structures)
        // -----------------------------------------------------------------
        SpawnBuilding(ctx, BuildingType::Main, 0, glm::vec3{-30.f, 0.f,  0.f});
        SpawnBuilding(ctx, BuildingType::Main, 1, glm::vec3{ 30.f, 0.f,  0.f});
        // Additional demo buildings
        SpawnBuilding(ctx, BuildingType::Attack, 0, glm::vec3{-20.f, 0.f,  12.f});
        SpawnBuilding(ctx, BuildingType::Defense, 1, glm::vec3{ 20.f, 0.f, -12.f});

        // -----------------------------------------------------------------
        // Spawn a few demo units per team so the system runs immediately
        // -----------------------------------------------------------------
        for (int i = 0; i < 3; i++) {
            SpawnUnit(ctx, 0, glm::vec3{-20.f + i * 3.f, 0.f,  5.f});
            SpawnUnit(ctx, 1, glm::vec3{ 20.f - i * 3.f, 0.f, -5.f});
        }

        /**
         * @brief Spawn a networked asset for testing purposes.
         */
        const uint32_t assetNetId = m_NextNetId++;
        
        // Authoritative server entity
        auto sEntity = ctx.serverRegistry.create();
        ctx.serverRegistry.emplace<TransformComponent>(sEntity, glm::vec3{2.f, 0.f, 2.f});
        ctx.serverRegistry.emplace<MovementComponent>(sEntity);
        ctx.serverRegistry.emplace<NetworkedComponent>(sEntity, assetNetId);
        ctx.serverRegistry.emplace<ModelComponent>(sEntity, "assets/test_scene_pixelation.glb");
        m_ServerNetMap[assetNetId] = sEntity;

        // Tell all clients (including ourselves) to spawn it visually
        AssetJoinedPacket pkt;
        pkt.netId = assetNetId;
        std::strncpy(pkt.modelPath, "assets/test_scene_pixelation.glb", sizeof(pkt.modelPath)-1);
        pkt.x = 2.f; pkt.y = 0.f; pkt.z = 2.f;
        ctx.network.BroadcastToAll(pkt);
        
        spdlog::info("GameScene: spawned test asset netId={}", assetNetId);

        /**
         * @brief Spawn an initial physics-driven cube.
         */
        SpawnPhysicsCube(ctx, glm::vec3{0.f, 10.f, 0.f});
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
    m_SnapAccum     = 0.f;
    m_SelectedUnits.clear();
    m_GameOver      = false;
    m_WinnerTeam    = 0xFFFFFFFFu;
    m_CameraMode    = CameraMode::Exploring;
    m_Camera->SetProjectionMode(ProjectionMode::Perspective);

    // Reset fog grid for next session
    m_Fog.Reset();

    // Reset client-side synced state
    m_ClientFog.Reset();
    m_HasFogData         = false;
    m_ClientTerritories.clear();
    m_HasTerritoryData   = false;
    m_TerritorySnapAccum = 0.f;
    m_FogSnapAccum       = 0.f;
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
void GameScene::Render(SceneContext& ctx, Renderer* renderer) {
    // ------------------------------------------------------------------
    // 1. Lazy-init graphics pipelines
    // ------------------------------------------------------------------
    if (!m_ModelPipeline) {
        ShaderResourceLayout vertLayout = {0, 0, 1, 1}; 
        ShaderResourceLayout fragLayout = {2, 0, 2, 1}; 

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

        ShaderResourceLayout shadowVertLayout = {0, 0, 0, 2};
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

    // 5b. Lights embedded inside models
    {
        auto modelView = ctx.clientRegistry.view<TransformComponent, ModelComponent>();
        for (auto entity : modelView) {
            if (totalLightCount >= 16) break;
            auto& transform = modelView.get<TransformComponent>(entity);
            auto& modelComp = modelView.get<ModelComponent>(entity);
            auto sceneData = AssetManager::LoadGLTF(modelComp.modelPath);
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
        if (!m_ShadowPipeline) return;
        renderCtx.BindPipeline(m_ShadowPipeline);
        renderCtx.PushVertexConstants(1, &sunVP, sizeof(glm::mat4));

        auto view = ctx.clientRegistry.view<TransformComponent, ModelComponent>();
        for (auto entity : view) {
            if (ctx.clientRegistry.any_of<GhostComponent>(entity)) continue;
            auto& transform = view.get<TransformComponent>(entity);
            auto& modelComp = view.get<ModelComponent>(entity);

            auto sceneData = AssetManager::LoadGLTF(modelComp.modelPath);
            if (!sceneData.model) continue;

            renderCtx.BindVertexBuffer(sceneData.model->GetVertexBuffer());
            renderCtx.BindIndexBuffer(sceneData.model->GetIndexBuffer());

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
    }, true, nullptr, 1.0f);

    // ------------------------------------------------------------------
    // 7. Render pass
    // ------------------------------------------------------------------
    Framebuffer* gbuffer = renderer->GetGBuffer();
    if (!gbuffer) return;

    renderer->AddPass("GameRenderPass", gbuffer, [this, ctx, renderer](RenderContext& renderCtx) {
        renderCtx.BindPipeline(m_ModelPipeline);
        renderCtx.BindVertexStorageBuffer(0, renderer->GetGlobalUBO());
        renderCtx.BindFragmentStorageBuffer(0, renderer->GetGlobalUBO());

        if (m_ShadowMap && m_ShadowMap->GetDepthTarget())
            renderCtx.BindFragmentTexture(1, m_ShadowMap->GetDepthTarget());

        // --- Render opaque entities ---
        auto view = ctx.clientRegistry.view<TransformComponent, ModelComponent>();
        for (auto entity : view) {
            if (ctx.clientRegistry.any_of<GhostComponent>(entity)) continue;
            auto& transform = view.get<TransformComponent>(entity);
            auto& modelComp = view.get<ModelComponent>(entity);

            auto sceneData = AssetManager::LoadGLTF(modelComp.modelPath);
            if (!sceneData.model) continue;

            renderCtx.BindVertexBuffer(sceneData.model->GetVertexBuffer());
            renderCtx.BindIndexBuffer(sceneData.model->GetIndexBuffer());

            if (sceneData.model->GetMaterialBuffer())
                renderCtx.BindFragmentStorageBuffer(1, sceneData.model->GetMaterialBuffer());

            const auto& allSections = sceneData.model->GetSections();
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
                    struct FragPC { uint32_t matIdx; uint32_t objID; float alpha; uint32_t pad; } fpc;
                    fpc.matIdx = (uint32_t)section.materialIndex;
                    fpc.objID  = 0;
                    fpc.alpha  = 1.0f;
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
                            struct FragPC { uint32_t matIdx; uint32_t objID; float alpha; uint32_t pad; } gfpc;
                            gfpc.matIdx = (uint32_t)section.materialIndex;
                            gfpc.objID  = 0;
                            gfpc.alpha  = 0.35f;
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
    }, true);
}

// ---------------------------------------------------------------------------
// Logic update
// ---------------------------------------------------------------------------

void GameScene::LogicUpdate(SceneContext& ctx, float dt) {
    if (!ctx.network.IsConnected()) {
        ctx.scenes.RequestTransition(new MainMenuScene());
        return;
    }

    // ------------------------------------------------------------------
    // TAB: cycle Exploring (1st-person) → Commander (top-down perspective)
    //       → Building (top-down orthographic) → Exploring
    // ------------------------------------------------------------------
    if (Input::IsKeyPressed(SDLK_TAB)) {
        switch (m_CameraMode) {
        case CameraMode::Exploring:
            // Save 1st-person state before switching away
            m_SavedExplorePos   = m_Camera->m_Position;
            m_SavedExploreYaw   = m_Camera->m_Yaw;
            m_SavedExplorePitch = m_Camera->m_Pitch;
            // Enter Commander
            m_CameraMode = CameraMode::Commander;
            m_Camera->SetProjectionMode(ProjectionMode::Perspective);
            m_Camera->m_Pitch = -89.f;
            m_Camera->m_Yaw   = -90.f;
            m_Camera->m_Position = glm::vec3(m_SavedExplorePos.x, m_TopDownHeight, m_SavedExplorePos.z);
            m_Camera->UpdateVectors();
            Input::SetRelativeMouseMode(ctx.renderer->GetWindow()->handle, false);
            spdlog::info("GameScene: switched to Commander mode");
            break;

        case CameraMode::Commander:
            // Save Commander position before switching to Building
            m_SavedCommanderPos = m_Camera->m_Position;
            // Enter Building
            m_CameraMode = CameraMode::Building;
            m_Camera->SetProjectionMode(ProjectionMode::Orthographic);
            m_Camera->m_OrthoSize = m_BuildOrthoSize;
            // Keep same XZ position but reset to building height
            m_Camera->m_Position.y = m_TopDownHeight;
            m_Camera->UpdateVectors();
            spdlog::info("GameScene: switched to Building mode (orthographic)");
            break;

        case CameraMode::Building:
            CancelPlacement(ctx);
            // Return to Exploring – restore 1st-person state
            m_CameraMode = CameraMode::Exploring;
            m_Camera->SetProjectionMode(ProjectionMode::Perspective);
            m_Camera->m_Position = m_SavedExplorePos;
            m_Camera->m_Yaw      = m_SavedExploreYaw;
            m_Camera->m_Pitch    = m_SavedExplorePitch;
            m_Camera->UpdateVectors();
            spdlog::info("GameScene: switched to Exploring mode");
            break;
        }
    }

    if (m_CameraMode == CameraMode::Exploring) {
        // ------------------------------------------------------------------
        // Exploring / 1st-Person mode
        // ------------------------------------------------------------------
        if (Input::IsKeyPressed(SDLK_F1)) {
            m_FreeFly = !m_FreeFly;
            spdlog::info("Control Mode: {}", m_FreeFly ? "Free Fly" : "Player");
            Input::SetRelativeMouseMode(ctx.renderer->GetWindow()->handle, true);
        }

        if (Input::IsKeyPressed(SDLK_F2)) {
            bool newState = !Input::IsRelativeMouseMode();
            Input::SetRelativeMouseMode(ctx.renderer->GetWindow()->handle, newState);
            spdlog::info("Mouse Capture: {}", newState ? "On" : "Off");
        }

        if (Input::IsRelativeMouseMode()) {
            glm::vec2 delta = Input::GetMouseDelta();
            m_Camera->Rotate(delta.x, delta.y);

            if (m_FreeFly) {
                if (Input::IsKeyDown(SDLK_W)) m_Camera->MoveForward(dt);
                if (Input::IsKeyDown(SDLK_S)) m_Camera->MoveBackward(dt);
                if (Input::IsKeyDown(SDLK_A)) m_Camera->MoveLeft(dt);
                if (Input::IsKeyDown(SDLK_D)) m_Camera->MoveRight(dt);
                if (Input::IsKeyDown(SDLK_SPACE)) m_Camera->MoveUp(dt);
                if (Input::IsKeyDown(SDLK_LSHIFT)) m_Camera->MoveDown(dt);
            } else {
                if (m_IdAssigned) {
                    auto view = ctx.clientRegistry.view<PlayerComponent, TransformComponent, MovementComponent>();
                    for (auto entity : view) {
                        auto& p = view.get<PlayerComponent>(entity);
                        if (p.isLocal) {
                            auto& tf = view.get<TransformComponent>(entity);
                            auto& mv = view.get<MovementComponent>(entity);

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
                            mv.velocity = moveDir * mv.speed;
                            tf.position += mv.velocity * dt;

                            m_Camera->m_Position = tf.position + glm::vec3(0, 2, 0);

                            tf.rotation.y = m_Camera->m_Yaw;
                            tf.rotation.x = m_Camera->m_Pitch;
                            break;
                        }
                    }
                }
            }
        }
    } else if (m_CameraMode == CameraMode::Commander || m_CameraMode == CameraMode::Building) {
        // ------------------------------------------------------------------
        // Top-down modes – Commander (perspective) and Building (orthographic)
        // ------------------------------------------------------------------
        int winW, winH;
        SDL_GetWindowSizeInPixels(ctx.renderer->GetWindow()->handle, &winW, &winH);

        // WASD pans the camera on XZ plane
        constexpr float TOPDOWN_PAN_SPEED = 25.f;
        glm::vec3 pan{0.f};
        if (Input::IsKeyDown(SDLK_W)) pan.z -= TOPDOWN_PAN_SPEED * dt;
        if (Input::IsKeyDown(SDLK_S)) pan.z += TOPDOWN_PAN_SPEED * dt;
        if (Input::IsKeyDown(SDLK_A)) pan.x -= TOPDOWN_PAN_SPEED * dt;
        if (Input::IsKeyDown(SDLK_D)) pan.x += TOPDOWN_PAN_SPEED * dt;
        m_Camera->m_Position += pan;
        // Keep camera looking straight down
        m_Camera->m_Pitch = -89.f;
        m_Camera->m_Yaw   = -90.f;
        m_Camera->UpdateVectors();

        // Building mode: scroll to zoom (adjust ortho size)
        if (m_CameraMode == CameraMode::Building) {
            float scroll = Input::GetMouseWheelDelta();
            if (scroll != 0.f) {
                m_BuildOrthoSize = glm::clamp(m_BuildOrthoSize - scroll * 3.f, 5.f, 150.f);
                m_Camera->m_OrthoSize = m_BuildOrthoSize;
                spdlog::debug("Building zoom: orthoSize={:.1f}", m_BuildOrthoSize);
            }

            // Placement ghost follow cursor
            if (m_PlacementActive && m_GhostEntity != entt::null &&
                ctx.clientRegistry.valid(m_GhostEntity)) {
                glm::vec2 mpos = Input::GetMousePosition();
                glm::vec3 worldPos = ScreenToWorldXZ(mpos.x, mpos.y, winW, winH);
                // Snap to 2x2 grid
                worldPos.x = std::round(worldPos.x / 2.f) * 2.f;
                worldPos.z = std::round(worldPos.z / 2.f) * 2.f;
                worldPos.y = 0.f;
                auto* tf = ctx.clientRegistry.try_get<TransformComponent>(m_GhostEntity);
                if (tf) tf->position = worldPos;

                // Left-click to place (skip if hovering over UI)
                if (Input::IsMouseButtonPressed(SDL_BUTTON_LEFT) &&
                    !ImGui::GetIO().WantCaptureMouse && ctx.network.IsHosting()) {
                    // Check no existing building at this position
                    bool blocked = false;
                    auto bldView = ctx.serverRegistry.view<BuildingComponent, TransformComponent>();
                    for (auto be : bldView) {
                        auto& btf = bldView.get<TransformComponent>(be);
                        auto& bc  = bldView.get<BuildingComponent>(be);
                        if (bc.destroyed) continue;
                        if (glm::distance(btf.position, worldPos) < 1.5f) { blocked = true; break; }
                    }
                    if (!blocked) {
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

                m_SelectedUnits.clear();

                auto view = ctx.clientRegistry.view<TransformComponent, NetworkedComponent, UnitComponent>();
                for (auto entity : view) {
                    auto& tf  = view.get<TransformComponent>(entity);
                    auto& nc  = view.get<NetworkedComponent>(entity);
                    auto& uc  = view.get<UnitComponent>(entity);
                    if (uc.teamId != m_MyPlayerId % 2) continue;
                    glm::vec2 d2 = glm::vec2(tf.position.x - worldPos.x, tf.position.z - worldPos.z);
                    if (glm::length(d2) <= SELECT_RADIUS) {
                        m_SelectedUnits.push_back(nc.netId);
                        uc.selected = true;
                    } else {
                        uc.selected = false;
                    }
                }
                spdlog::info("Commander: selected {} unit(s)", m_SelectedUnits.size());
            }

            // Right-click: issue move order to selected units
            if (Input::IsMouseButtonPressed(SDL_BUTTON_RIGHT) && !m_SelectedUnits.empty()) {
                glm::vec2 mpos     = Input::GetMousePosition();
                glm::vec3 worldPos = ScreenToWorldXZ(mpos.x, mpos.y, winW, winH);

                CommanderOrderPacket pkt;
                pkt.playerId = m_MyPlayerId;
                pkt.x = worldPos.x; pkt.y = worldPos.y; pkt.z = worldPos.z;
                pkt.selectedCount = std::min(static_cast<uint32_t>(m_SelectedUnits.size()), 32u);
                for (uint32_t i = 0; i < pkt.selectedCount; ++i)
                    pkt.selectedNetIds[i] = m_SelectedUnits[i];
                ctx.network.Send(pkt);
                spdlog::info("Commander: move order to ({:.1f},{:.1f},{:.1f}) for {} units",
                             worldPos.x, worldPos.y, worldPos.z, pkt.selectedCount);
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
    m_TotalTime += dt;
    m_FrameCount++;
}

void GameScene::UIUpdate(SceneContext& ctx, float dt) {
    // ------------------------------------------------------------------
    // Game-over overlay
    // ------------------------------------------------------------------
    if (m_GameOver) {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 180));
        ImGui::Begin("Spielende", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        if (m_WinnerTeam == m_MyPlayerId % 2)
            ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "SIEG! Team %u gewinnt!", m_WinnerTeam);
        else if (m_WinnerTeam == 0xFFFFFFFFu)
            ImGui::Text("Unentschieden!");
        else
            ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "NIEDERLAGE! Team %u gewinnt.", m_WinnerTeam);
        if (ImGui::Button("Zum Hauptmenü")) {
            ctx.network.Disconnect();
            ctx.scenes.RequestTransition(new MainMenuScene());
        }
        ImGui::End();
        return;
    }

    ImGui::Begin("Game");
    const char* modeLabel = "Unknown";
    switch (m_CameraMode) {
        case CameraMode::Exploring: modeLabel = "Exploring (1st-Person)"; break;
        case CameraMode::Commander: modeLabel = "Commander (Top-Down)";   break;
        case CameraMode::Building:  modeLabel = "Building (Ortho)";      break;
    }
    ImGui::Text("Mode: %s | %s", ctx.network.IsHosting() ? "Host" : "Client", modeLabel);
    ImGui::Text("[TAB] wechseln");
    if (m_CameraMode == CameraMode::Exploring)
        ImGui::Text("Control: %s (F1)", m_FreeFly ? "Free Fly" : "Player");
    ImGui::Text("Mouse: %s (F2)", Input::IsRelativeMouseMode() ? "Captured" : "Visible");
    if (m_IdAssigned)
        ImGui::Text("playerId=%u  netId=%u", m_MyPlayerId, m_MyNetId);
    else
        ImGui::Text("Waiting for server assignment...");

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

        // --- Generic buildings row ---
        struct BldBtn {
            const char* name;
            BuildingType type;
        };
        const BldBtn genericBlds[] = {
            {"Basis",      BuildingType::Main},
            {"Speicher",   BuildingType::Storage},
            {"Brutkammer", BuildingType::Barracks},
            {"Upgrade",    BuildingType::Upgrade},
            {"Konversion", BuildingType::Conversion},
            {"Verteid.",   BuildingType::Defense},
            {"Angriff",    BuildingType::Attack},
            {"Vorposten",  BuildingType::Outpost},
        };

        ImGui::Text("Gebaeude platzieren:");
        for (int i = 0; i < 8; i++) {
            bool active = (m_PlacementActive &&
                           m_SelectedBuildingType == static_cast<int>(genericBlds[i].type));
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.f));
            if (ImGui::Button(genericBlds[i].name)) {
                if (active) CancelPlacement(ctx);
                else        EnterPlacementMode(ctx, genericBlds[i].type);
            }
            if (active) ImGui::PopStyleColor();
            if ((i + 1) % 4 != 0) ImGui::SameLine();
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
            uint32_t myTeam = m_MyPlayerId % 2;
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
        }
    }

    if (ctx.network.IsHosting()) {
        if (ImGui::Button("Spawn Physics Cube")) {
            SpawnPhysicsCube(ctx, m_Camera->m_Position + m_Camera->m_Front * 5.0f);
        }
        if (ImGui::Button("Spawn Unit (Team 0)")) {
            glm::vec3 sp = RandomSpawnInTerritory(ctx, 0);
            SpawnUnit(ctx, 0, sp);
        }
        ImGui::SameLine();
        if (ImGui::Button("Spawn Unit (Team 1)")) {
            glm::vec3 sp = RandomSpawnInTerritory(ctx, 1);
            SpawnUnit(ctx, 1, sp);
        }
    }

    if (ImGui::Button("Disconnect")) {
        ctx.network.Disconnect();
        ctx.scenes.RequestTransition(new MainMenuScene());
    }
    ImGui::End();

    // Resource stockpile HUD
    if (ctx.network.IsHosting()) {
        ResourceHUD::Draw(ctx.serverRegistry, /*teamId=*/0);
    }

    // Map overlay (available to all clients with synced data)
    {
        ImGui::Begin("Game");
        if (ImGui::Button(m_ShowMapOverlay ? "Karte schliessen" : "Karte [M]"))
            m_ShowMapOverlay = !m_ShowMapOverlay;
        ImGui::End();

        if (ImGui::IsKeyPressed(ImGuiKey_M))
            m_ShowMapOverlay = !m_ShowMapOverlay;

        if (m_ShowMapOverlay) {
            ImGuiIO& io = ImGui::GetIO();
            ImVec2 mapOrigin(0.f, 0.f);
            ImVec2 mapSize(io.DisplaySize.x, io.DisplaySize.y);

            // Draw fog overlay (server's authoritative grid or client's synced copy)
            if (ctx.network.IsHosting()) {
                FogOfWarSystem::DrawOverlay(m_Fog, mapOrigin, mapSize);
            } else if (m_HasFogData) {
                FogOfWarSystem::DrawOverlay(m_ClientFog, mapOrigin, mapSize);
            }

            // Draw territory overlay (server registry or synced packet data)
            if (ctx.network.IsHosting()) {
                TerritorySystem::DrawOverlay(ctx.serverRegistry, mapOrigin, mapSize,
                    glm::vec2{-50.f, -50.f}, glm::vec2{50.f, 50.f});
            } else if (m_HasTerritoryData) {
                // Draw from client-side territory data using a minimal inline overlay
                DrawClientTerritoryOverlay(mapOrigin, mapSize);
            }
        }
    }

    // HP bars above units (visible in both top-down modes)
    if (m_CameraMode == CameraMode::Commander || m_CameraMode == CameraMode::Building) {
        DrawUnitHPBars(ctx);
    }

    ImGui::Begin("Shadow Debug");
    ImGui::SliderFloat("Bias Constant", &m_ShadowBiasConstant, 0.0f, 10.0f);
    ImGui::SliderFloat("Bias Slope", &m_ShadowBiasSlope, 0.0f, 10.0f);
    ImGui::SliderFloat("Ortho Size", &m_ShadowOrthoSize, 5.0f, 100.0f);
    ImGui::End();
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

        // Process commander movement orders from clients
        while (true) {
            auto result = ctx.network.ReceiveFromClient<CommanderOrderPacket>(PacketType::COMMANDER_ORDER);
            if (!result) break;
            auto [pkt, senderPeer] = *result;
            glm::vec3 dest{pkt.x, pkt.y, pkt.z};
            spdlog::info("Commander order playerId={}: ({:.1f},{:.1f},{:.1f}) {} units",
                         pkt.playerId, dest.x, dest.y, dest.z, pkt.selectedCount);
            // Apply order to the specific units identified by netId in the packet
            uint32_t count = std::min(pkt.selectedCount, 32u);
            for (uint32_t i = 0; i < count; ++i) {
                auto it = m_ServerNetMap.find(pkt.selectedNetIds[i]);
                if (it == m_ServerNetMap.end()) continue;
                auto* mo = ctx.serverRegistry.try_get<MovementOrderComponent>(it->second);
                if (mo) {
                    mo->destination = dest;
                    mo->active      = true;
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
        FogOfWarSystem::Update(m_Fog, ctx.serverRegistry);
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
                uint32_t teamId = newPlayerId % 2;
                auto& state = UpgradeSystem::GetState(teamId);
                for (auto pid : state.completedPaths) {
                    UpgradeCompletedPacket upkt;
                    upkt.pathId = pid;
                    upkt.teamId = teamId;
                    ctx.network.SendToClient(peerId, upkt);
                }
            }

            // Send initial territory + fog snapshots to the new client
            SendTerritorySnapshot(ctx);
            SendFogSnapshot(ctx);

            // Tell the new client their identity
            PlayerIdAssignPacket idPkt;
            idPkt.playerId = newPlayerId;
            idPkt.netId    = newNetId;
            ctx.network.SendToClient(peerId, idPkt);

            // Create the server entity
            auto entity = ctx.serverRegistry.create();
            ctx.serverRegistry.emplace<TransformComponent>(entity);
            ctx.serverRegistry.emplace<MovementComponent>(entity);
            ctx.serverRegistry.emplace<PlayerComponent>(entity, newPlayerId, false);
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
        if (ctx.network.IsHosting()) continue;

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
        auto entity = ctx.clientRegistry.create();
        ctx.clientRegistry.emplace<TransformComponent>(entity, glm::vec3{pkt->x, pkt->y, pkt->z});
        ctx.clientRegistry.emplace<MovementComponent>(entity);
        ctx.clientRegistry.emplace<NetworkedComponent>(entity, pkt->netId);
        ctx.clientRegistry.emplace<ModelComponent>(entity, std::string("assets/cube.glb"));
        ctx.clientRegistry.emplace<UnitComponent>(entity,
            UnitComponent{pkt->teamId, static_cast<BugClass>(pkt->bugClass), false});
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

    // Fog snapshot
    {
        auto pkt = ctx.network.ReceiveFromServer<FogSnapshotPacket>(PacketType::FOG_SNAPSHOT);
        if (pkt) HandleFogSnapshot(*pkt);
    }

    // Resource spawned
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<ResourceSpawnedPacket>(PacketType::RESOURCE_SPAWNED);
        if (!pkt) break;
        if (m_ClientNetMap.count(pkt->netId)) continue;
        auto entity = ctx.clientRegistry.create();
        ctx.clientRegistry.emplace<TransformComponent>(entity, glm::vec3{pkt->x, pkt->y, pkt->z});
        ctx.clientRegistry.emplace<NetworkedComponent>(entity, pkt->netId);
        ctx.clientRegistry.emplace<ModelComponent>(entity, std::string("assets/cube.glb"));
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

    // Game over
    {
        auto pkt = ctx.network.ReceiveFromServer<GameOverPacket>(PacketType::GAME_OVER);
        if (pkt && !m_GameOver) {
            m_GameOver    = true;
            m_WinnerTeam  = pkt->winnerTeam;
            spdlog::info("GameScene: GAME OVER – winner team {}", m_WinnerTeam);
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
    if (!ctx.network.IsConnected() || !Input::IsRelativeMouseMode()) return;
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
void GameScene::SpawnUnit(SceneContext& ctx, uint32_t teamId, glm::vec3 pos, float hp)
{
    if (!ctx.network.IsHosting()) return;

    static std::mt19937 rng{std::random_device{}()};
    // Pick a random bug class from available ones (skip None)
    static const BugClass classes[] = {
        BugClass::Ants, BugClass::Beetles, BugClass::Mantis,
        BugClass::Dragonflies, BugClass::Roaches, BugClass::Scorpions
    };
    BugClass bc = classes[rng() % std::size(classes)];

    // Damage by diet archetype
    float dmg = 7.f;
    if (bc == BugClass::Mantis || bc == BugClass::Dragonflies || bc == BugClass::Scorpions)
        dmg = 20.f;
    else if (bc == BugClass::Ants || bc == BugClass::Roaches || bc == BugClass::Beetles ||
             bc == BugClass::CentipedesWorms)
        dmg = 12.f;

    const uint32_t netId = m_NextNetId++;
    auto e = ctx.serverRegistry.create();
    ctx.serverRegistry.emplace<TransformComponent>(e, pos);
    ctx.serverRegistry.emplace<MovementComponent>(e);
    ctx.serverRegistry.emplace<NetworkedComponent>(e, netId);
    ctx.serverRegistry.emplace<ModelComponent>(e, std::string("assets/cube.glb"));
    ctx.serverRegistry.emplace<UnitComponent>(e, UnitComponent{teamId, bc, false});
    ctx.serverRegistry.emplace<HealthComponent>(e, HealthComponent{hp});
    ctx.serverRegistry.emplace<CombatComponent>(e,
        CombatComponent{/*range=*/6.f, /*dmg=*/dmg, /*cd=*/0.f, /*rate=*/1.5f, entt::null});
    ctx.serverRegistry.emplace<MovementOrderComponent>(e);
    m_ServerNetMap[netId] = e;

    UnitSpawnedPacket pkt;
    pkt.netId    = netId;
    pkt.teamId   = teamId;
    pkt.bugClass = static_cast<uint8_t>(bc);
    pkt.x = pos.x; pkt.y = pos.y; pkt.z = pos.z;
    pkt.hp = hp; pkt.maxHp = hp;
    ctx.network.BroadcastToAll(pkt);

    spdlog::info("GameScene: spawned unit netId={} team={} class={} hp={:.0f}",
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

    // Determine the owner's bug class from the first player of this team
    BugClass ownerClass = BugClass::Termites;
    {
        auto pView = ctx.serverRegistry.view<PlayerComponent>();
        for (auto pe : pView) {
            auto& pc = pView.get<PlayerComponent>(pe);
            if (pc.playerId % 2 == teamId % 2 && pc.bugClass != BugClass::None) {
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
        case BuildingType::Storage:
            ctx.serverRegistry.emplace<ResourceInventory>(e);
            if (type == BuildingType::Storage)
                ctx.serverRegistry.emplace<StorageComponent>(e);
            break;
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
    m_SelectedBuildingType = -1;
}

// ---------------------------------------------------------------------------
// RandomSpawnInTerritory
// ---------------------------------------------------------------------------

glm::vec3 GameScene::RandomSpawnInTerritory(SceneContext& ctx, uint32_t teamId)
{
    // Gather all territory zone centres and pick one that roughly belongs
    // to the given team (simple parity: even zones → team 0, odd → team 1).
    struct ZoneInfo { glm::vec3 center; float hw, hd; };
    std::vector<ZoneInfo> candidates;

    auto view = ctx.serverRegistry.view<TransformComponent, TerritoryComponent>();
    uint32_t idx = 0;
    for (auto e : view) {
        const auto& tf  = view.get<TransformComponent>(e);
        const auto& ter = view.get<TerritoryComponent>(e);
        if (idx % 2 == teamId % 2)
            candidates.push_back({tf.position, ter.halfW, ter.halfD});
        idx++;
    }

    // Fallback: spread by team
    if (candidates.empty()) {
        float x = (teamId == 0) ? -20.f : 20.f;
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
    auto view = ctx.serverRegistry.view<TransformComponent, MovementComponent,
                                        MovementOrderComponent, UnitComponent>();
    for (auto e : view) {
        auto& tf = view.get<TransformComponent>(e);
        auto& mv = view.get<MovementComponent>(e);
        auto& mo = view.get<MovementOrderComponent>(e);

        if (!mo.active) { mv.velocity = {0.f, 0.f, 0.f}; continue; }

        glm::vec3 dir = mo.destination - tf.position;
        dir.y = 0.f; // stay on ground
        float dist = glm::length(dir);
        if (dist < 1.f) {
            mo.active    = false;
            mv.velocity  = {0.f, 0.f, 0.f};
        } else {
            dir = glm::normalize(dir);
            mv.velocity  = dir * mv.speed;
            tf.position += mv.velocity * dt;
            tf.rotation.y = glm::degrees(std::atan2(dir.x, dir.z));
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
            if (!thc || thc->dead) { targetValid = false; cc.target = entt::null; }
        }

        // Find nearest enemy if no valid target
        if (!targetValid) {
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
        if (!ttf) { cc.target = entt::null; continue; }
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
    if (surviving.empty()) {
        auto view = ctx.serverRegistry.view<BaseHealthComponent>();
        for (auto e : view) {
            const auto& bhc = view.get<BaseHealthComponent>(e);
            if (!bhc.destroyed) surviving[bhc.teamId]++;
        }
    }

    if (surviving.empty()) return;

    // Find teams whose Main buildings are all gone
    std::vector<uint32_t> eliminated;
    for (const auto& [team, cnt] : surviving)
        if (cnt == 0) eliminated.push_back(team);

    if (eliminated.empty()) return;

    // Determine winner (the non-eliminated team)
    uint32_t winner = 0xFFFFFFFFu;
    for (const auto& [team, cnt] : surviving)
        if (cnt > 0) { winner = team; break; }

    m_GameOver   = true;
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

    // Intersect with Y=0 plane
    if (std::abs(rayDir.y) < 1e-6f) return {0.f, 0.f, 0.f};
    float t = -rayOrig.y / rayDir.y;
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
    int winW, winH;
    SDL_GetWindowSizeInPixels(ctx.renderer->GetWindow()->handle, &winW, &winH);
    if (winW <= 0 || winH <= 0) return;

    float aspect = (float)winW / (float)winH;
    glm::mat4 vp = m_Camera->GetProjectionMatrix(aspect) * m_Camera->GetViewMatrix();

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
    if (!ctx.network.IsHosting() || !m_Fog.IsInitialised()) return;

    FogSnapshotPacket pkt;
    pkt.cellsX = static_cast<uint16_t>(m_Fog.cellsX);
    pkt.cellsZ = static_cast<uint16_t>(m_Fog.cellsZ);

    size_t totalCells = static_cast<size_t>(m_Fog.cellsX) * static_cast<size_t>(m_Fog.cellsZ);
    size_t words = (totalCells + 63) / 64;
    if (words > 160) words = 160;

    for (size_t w = 0; w < words; ++w) {
        uint64_t bits = 0;
        for (size_t b = 0; b < 64; ++b) {
            size_t idx = w * 64 + b;
            if (idx < totalCells && m_Fog.revealed[idx])
                bits |= (uint64_t(1) << b);
        }
        pkt.gridData[w] = bits;
    }

    ctx.network.BroadcastToAll(pkt);
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
    m_HasTerritoryData = true;
}

void GameScene::HandleFogSnapshot(const FogSnapshotPacket& pkt)
{
    if (pkt.cellsX == 0 || pkt.cellsZ == 0) return;

    // Initialise client fog grid with matching dimensions
    m_ClientFog.Init(
        glm::vec3{-50.f, 0.f, -50.f},
        glm::vec3{ 50.f, 0.f,  50.f},
        /*cellSize=*/2.f
    );

    size_t totalCells = static_cast<size_t>(pkt.cellsX) * static_cast<size_t>(pkt.cellsZ);
    size_t words = (totalCells + 63) / 64;
    if (words > 160) words = 160;

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

// ---------------------------------------------------------------------------
// Client-side territory overlay drawing from synced packet data
// ---------------------------------------------------------------------------

void GameScene::DrawClientTerritoryOverlay(ImVec2 mapOriginPx, ImVec2 mapSizePx)
{
    const glm::vec2 worldMin{-50.f, -50.f};
    const glm::vec2 worldMax{ 50.f,  50.f};

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration      |
        ImGuiWindowFlags_NoInputs          |
        ImGuiWindowFlags_NoNav             |
        ImGuiWindowFlags_NoMove            |
        ImGuiWindowFlags_NoSavedSettings   |
        ImGuiWindowFlags_NoFocusOnAppearing|
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::SetNextWindowPos(mapOriginPx, ImGuiCond_Always);
    ImGui::SetNextWindowSize(mapSizePx,  ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

    if (ImGui::Begin("##ClientTerritory", nullptr, kFlags))
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        auto ToScreen = [&](float wx, float wz) -> ImVec2 {
            float nx = (wx - worldMin.x) / (worldMax.x - worldMin.x);
            float nz = (wz - worldMin.y) / (worldMax.y - worldMin.y);
            return ImVec2(mapOriginPx.x + nx * mapSizePx.x,
                          mapOriginPx.y + nz * mapSizePx.y);
        };

        for (const auto& cz : m_ClientTerritories)
        {
            ImVec2 tl = ToScreen(cz.center.x - cz.halfW, cz.center.z - cz.halfD);
            ImVec2 br = ToScreen(cz.center.x + cz.halfW, cz.center.z + cz.halfD);

            ImU32 fillCol;
            if (cz.ownerTeam != 0xFFFF'FFFFu)
                fillCol = TerritoryColors::ForTeamU32(cz.ownerTeam, 0.35f);
            else
                fillCol = IM_COL32(180, 180, 180, 60);

            dl->AddRectFilled(tl, br, fillCol, 4.f);

            if (cz.contestedBy != 0xFFFF'FFFFu && cz.captureTime > 0.f)
            {
                float pct = cz.captureProgress / cz.captureTime;
                ImVec2 barTL(tl.x, br.y - 4.f);
                ImVec2 barBR(tl.x + (br.x - tl.x) * pct, br.y);
                dl->AddRectFilled(barTL, barBR,
                    TerritoryColors::ForTeamU32(cz.contestedBy, 0.9f));
            }

            ImU32 borderCol = (cz.contestedBy != 0xFFFF'FFFFu)
                ? TerritoryColors::ForTeamU32(cz.contestedBy, 1.f)
                : IM_COL32(255, 255, 255, 120);
            dl->AddRect(tl, br, borderCol, 4.f, 0, 1.5f);

            ImVec2 labelPos(
                (tl.x + br.x) * 0.5f - ImGui::CalcTextSize(cz.name).x * 0.5f,
                (tl.y + br.y) * 0.5f - 6.f);
            dl->AddText(labelPos, IM_COL32(255, 255, 255, 200), cz.name);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}
