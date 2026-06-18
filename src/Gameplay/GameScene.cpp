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

void GameScene::OnEnter(SceneContext& ctx) {
    spdlog::info("GameScene: entered");

    if (!m_Camera) {
        m_Camera = std::make_unique<Camera>();
        m_Camera->m_Position = {0, 2, 10};
        m_Camera->m_Yaw = -90.0f;
        m_Camera->m_Pitch = 0.0f;
        m_Camera->UpdateVectors();
    }

    if (ctx.network.IsHosting()) {
        LoadSceneMeshCollision(ctx, "assets/test_scene_pixelation.glb");
        m_ResourceManager.Init();
        m_ResourceManager.SpawnPermanentResources(ctx.serverRegistry);
        m_ResourceManager.SpawnMeatDrop(ctx.serverRegistry, glm::vec3{0.f, 0.f, 3.f}, 2, 15.f);
        m_Fog.Init(glm::vec3{-50.f, 0.f, -50.f}, glm::vec3{ 50.f, 0.f,  50.f}, 2.f);
        TerritorySystem::SpawnZones(ctx.serverRegistry);
        HUDTextures::Load(ctx.renderer->GetDevice());

        auto spawnBase = [&](uint32_t team, glm::vec3 pos) {
            const uint32_t netId = m_NextNetId++;
            auto e = ctx.serverRegistry.create();
            ctx.serverRegistry.emplace<TransformComponent>(e, pos);
            ctx.serverRegistry.emplace<BaseComponent>(e, team);
            ctx.serverRegistry.emplace<BaseHealthComponent>(e, BaseHealthComponent{team, 500.f, 500.f, false});
            ctx.serverRegistry.emplace<NetworkedComponent>(e, netId);
            ctx.serverRegistry.emplace<ModelComponent>(e, std::string("assets/cube.glb"));
            m_ServerNetMap[netId] = e;
            AssetJoinedPacket bp;
            bp.netId = netId;
            std::strncpy(bp.modelPath, "assets/cube.glb", sizeof(bp.modelPath)-1);
            bp.x = pos.x; bp.y = pos.y; bp.z = pos.z;
            ctx.network.BroadcastToAll(bp);
            BaseSpawnedPacket bsp;
            bsp.netId  = netId;
            bsp.teamId = team;
            bsp.hp     = 500.f;
            bsp.maxHp  = 500.f;
            ctx.network.BroadcastToAll(bsp);
        };
        spawnBase(0, glm::vec3{-30.f, 0.f,  0.f});
        spawnBase(1, glm::vec3{ 30.f, 0.f,  0.f});

        for (int i = 0; i < 3; i++) {
            SpawnUnit(ctx, 0, glm::vec3{-20.f + i * 3.f, 0.f,  5.f});
            SpawnUnit(ctx, 1, glm::vec3{ 20.f - i * 3.f, 0.f, -5.f});
        }

        const uint32_t assetNetId = m_NextNetId++;
        auto sEntity = ctx.serverRegistry.create();
        ctx.serverRegistry.emplace<TransformComponent>(sEntity, glm::vec3{2.f, 0.f, 2.f});
        ctx.serverRegistry.emplace<MovementComponent>(sEntity);
        ctx.serverRegistry.emplace<NetworkedComponent>(sEntity, assetNetId);
        ctx.serverRegistry.emplace<ModelComponent>(sEntity, "assets/test_scene_pixelation.glb");
        m_ServerNetMap[assetNetId] = sEntity;

        AssetJoinedPacket pkt;
        pkt.netId = assetNetId;
        std::strncpy(pkt.modelPath, "assets/test_scene_pixelation.glb", sizeof(pkt.modelPath)-1);
        pkt.x = 2.f; pkt.y = 0.f; pkt.z = 2.f;
        ctx.network.BroadcastToAll(pkt);
        SpawnPhysicsCube(ctx, glm::vec3{0.f, 10.f, 0.f});
    }
}

void GameScene::OnExit(SceneContext& ctx) {
    if (ctx.network.IsHosting()) {
        auto view = ctx.serverRegistry.view<PhysicsBodyComponent>();
        for (auto entity : view) {
            auto& body = view.get<PhysicsBodyComponent>(entity);
            if (ctx.physics && body.handle.IsValid()) {
                uint32_t physicsId = static_cast<uint32_t>(ctx.physics->GetSystem().GetBodyInterface().GetUserData(body.handle.id));
                ctx.world->UnregisterPhysicsEntity(physicsId);
                ctx.physics->RemoveBody(body.handle);
            }
        }
        if (ctx.physics) {
            for (auto& meshBody : m_MeshCollisionBodies) {
                if (meshBody.IsValid()) ctx.physics->RemoveBody(meshBody);
            }
        }
        m_MeshCollisionBodies.clear();
    }
    ctx.serverRegistry.clear();
    ctx.clientRegistry.clear();
    m_ServerNetMap.clear();
    m_PeerToNetId.clear();
    m_ClientNetMap.clear();
    m_NextNetId = 1;
    m_NextPlayerId = 0;
    m_MyPlayerId = 0;
    m_MyNetId = 0;
    m_IdAssigned = false;
    m_SnapAccum = 0.f;
    m_SelectedUnits.clear();
    m_GameOver = false;
    m_WinnerTeam = 0xFFFFFFFFu;
    m_CameraMode = CameraMode::Commander;
    m_Fog.Reset();
    HUDTextures::Unload();
    ctx.world->ClearPhysicsState();
    m_VertShader.reset();
    m_FragShader.reset();
    m_ModelPipeline = nullptr;
    // m_GhostVertShader.reset();
    // m_GhostFragShader.reset();
    // m_GhostPipeline = nullptr;
    // m_GhostVertexBuffer.reset();
    // m_GhostIndexBuffer.reset();
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

void GameScene::Render(SceneContext& ctx, Renderer* renderer) {
    if (!m_ModelPipeline) {
        ShaderResourceLayout vertLayout = {0, 0, 1, 1}; 
        ShaderResourceLayout fragLayout = {2, 0, 2, 1}; 
        m_VertShader = std::make_unique<Shader>(renderer->GetDevice(), "shaders/model.vert.spv", ShaderStage::Vertex, vertLayout);
        m_FragShader = std::make_unique<Shader>(renderer->GetDevice(), "shaders/model.frag.spv", ShaderStage::Fragment, fragLayout);
        PipelineConfig config;
        config.vertexShader = m_VertShader.get();
        config.fragmentShader = m_FragShader.get();
        config.vertexStride = sizeof(float) * 3; // Simplified for mesh vertex loading
        config.vertexAttributes = {{0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0}};
        config.enableDepthTest = true;
        config.depthCompareOp = SDL_GPU_COMPAREOP_LESS;
        config.cullMode = SDL_GPU_CULLMODE_BACK;
        config.colorTargetFormats = {
            SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
            SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
            SDL_GPU_TEXTUREFORMAT_R16_UINT
        };
        m_ModelPipeline = renderer->GetPipelines()->CreatePipeline("ModelPipeline", config, SDL_GPU_TEXTUREFORMAT_INVALID);
    }

    if (!m_ShadowPipeline) {
        ShaderResourceLayout shadowVertLayout = {0, 0, 0, 1};
        ShaderResourceLayout shadowFragLayout = {0, 0, 0, 0};
        m_ShadowVertShader = std::make_unique<Shader>(renderer->GetDevice(), "shaders/shadow.vert.spv", ShaderStage::Vertex, shadowVertLayout);
        m_ShadowFragShader = std::make_unique<Shader>(renderer->GetDevice(), "shaders/shadow.frag.spv", ShaderStage::Fragment, shadowFragLayout);
        PipelineConfig shadowCfg;
        shadowCfg.vertexShader = m_ShadowVertShader.get();
        shadowCfg.fragmentShader = m_ShadowFragShader.get();
        shadowCfg.vertexStride = sizeof(float) * 3;
        shadowCfg.vertexAttributes = {{0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0}};
        shadowCfg.enableDepthTest = true;
        shadowCfg.depthCompareOp = SDL_GPU_COMPAREOP_LESS;
        shadowCfg.cullMode = SDL_GPU_CULLMODE_FRONT;
        shadowCfg.enableDepthBias = true;
        shadowCfg.depthBiasConstantFactor = m_ShadowBiasConstant;
        shadowCfg.depthBiasSlopeFactor = m_ShadowBiasSlope;
        m_ShadowPipeline = renderer->GetPipelines()->CreatePipeline("ShadowPipeline", shadowCfg, SDL_GPU_TEXTUREFORMAT_INVALID);
        m_LastShadowBiasConstant = m_ShadowBiasConstant;
        m_LastShadowBiasSlope = m_ShadowBiasSlope;
    }

    if (m_ShadowBiasConstant != m_LastShadowBiasConstant || m_ShadowBiasSlope != m_LastShadowBiasSlope) {
        PipelineConfig shadowCfg;
        shadowCfg.vertexShader = m_ShadowVertShader.get();
        shadowCfg.fragmentShader = m_ShadowFragShader.get();
        shadowCfg.vertexStride = sizeof(float) * 3;
        shadowCfg.vertexAttributes = {{0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0}};
        shadowCfg.enableDepthTest = true;
        shadowCfg.depthCompareOp = SDL_GPU_COMPAREOP_LESS;
        shadowCfg.cullMode = SDL_GPU_CULLMODE_FRONT;
        shadowCfg.enableDepthBias = true;
        shadowCfg.depthBiasConstantFactor = m_ShadowBiasConstant;
        shadowCfg.depthBiasSlopeFactor = m_ShadowBiasSlope;
        renderer->FlushAndWait();
        m_ShadowPipeline = renderer->GetPipelines()->CreatePipeline("ShadowPipeline_" + std::to_string(m_FrameCount), shadowCfg, SDL_GPU_TEXTUREFORMAT_INVALID);
        m_LastShadowBiasConstant = m_ShadowBiasConstant;
        m_LastShadowBiasSlope = m_ShadowBiasSlope;
        renderer->RestartImGuiFrame();
    }

    if (!m_ShadowMap) {
        uint32_t shadowRes = 2048;
        m_ShadowMap = std::make_unique<Framebuffer>(renderer->GetDevice(), shadowRes, shadowRes, std::vector<SDL_GPUTextureFormat>{}, true);
        m_ShadowUBO = std::make_unique<GPUBuffer>(renderer->GetDevice(), BufferUsage::Uniform, sizeof(glm::mat4));
    }

    m_TotalTime += ctx.world->GetAccumulator(); // Use world accumulator for time
    m_FrameCount++;
    auto& globals = renderer->GetGlobalUniforms();
    globals.view = m_Camera->GetViewMatrix();
    int winW, winH;
    SDL_GetWindowSizeInPixels(renderer->GetWindow()->handle, &winW, &winH);
    float aspect = (winW > 0) ? (float)winW / (float)winH : 1.f;
    globals.proj = m_Camera->GetProjectionMatrix(aspect);
    globals.viewProj = globals.proj * globals.view;
    glm::vec3 sunTarget = m_Camera->m_Position;
    sunTarget.y = 0.f;
    glm::vec3 sunPos = sunTarget - m_SunDirection * 40.f;
    glm::mat4 sunView = glm::lookAt(sunPos, sunTarget, glm::vec3(0, 1, 0));
    float sSize = m_ShadowOrthoSize;
    glm::mat4 sunProj = glm::ortho(-sSize, sSize, -sSize, sSize, 0.1f, 100.f);
    globals.sunVP = sunProj * sunView;
    globals.sunColor = glm::vec4(m_SunColor, m_SunIntensity);
    globals.sunDir = glm::vec4(m_SunDirection, 0.f);
    globals.cameraPos = glm::vec4(m_Camera->m_Position, 1.f);
    globals.ambientColor = glm::vec4(m_AmbientColor, m_AmbientIntensity);
    globals.timers = glm::vec4(m_TotalTime, 0.f, 0.016f, (float)m_FrameCount);
    renderer->UpdateGlobalUniforms(globals);

    renderer->AddPass("ShadowPass", m_ShadowMap.get(), [this, &clientReg = ctx.clientRegistry, renderer](RenderContext& renderCtx) {
        renderCtx.BindPipeline(m_ShadowPipeline);
        renderCtx.BindVertexStorageBuffer(0, renderer->GetGlobalUBO());
        auto view = clientReg.view<TransformComponent, ModelComponent>();
        for (auto entity : view) {
            auto& tf = view.get<TransformComponent>(entity);
            auto& mod = view.get<ModelComponent>(entity);
            auto asset = AssetManager::LoadModel(mod.modelPath);
            if (!asset) continue;
            glm::mat4 modelMatrix = glm::translate(glm::mat4(1.f), tf.position) * 
                                    glm::mat4_cast(glm::quat(glm::radians(tf.rotation))) * 
                                    glm::scale(glm::mat4(1.f), tf.scale);
            renderCtx.PushVertexConstants(0, &modelMatrix, sizeof(glm::mat4));
            renderCtx.BindVertexBuffer(asset->GetVertexBuffer());
            renderCtx.BindIndexBuffer(asset->GetIndexBuffer());
            for (const auto& section : asset->GetSections()) {
                renderCtx.DrawIndexed(section.indexCount, 1, section.firstIndex);
            }
        }
    }, true, nullptr, 1.0f);

    auto gbuffer = renderer->GetGBuffer();
    renderer->AddPass("GeometryPass", gbuffer, [this, renderer, &clientReg = ctx.clientRegistry](RenderContext& renderCtx) {
        renderCtx.BindPipeline(m_ModelPipeline);
        renderCtx.BindVertexStorageBuffer(0, renderer->GetGlobalUBO());
        renderCtx.BindFragmentStorageBuffer(2, renderer->GetGlobalUBO());
        renderCtx.BindFragmentTexture(1, m_ShadowMap->GetDepthTarget());
        auto view = clientReg.view<TransformComponent, ModelComponent>();
        for (auto entity : view) {
            auto& tf = view.get<TransformComponent>(entity);
            auto& mod = view.get<ModelComponent>(entity);
            auto asset = AssetManager::LoadModel(mod.modelPath);
            if (!asset) continue;
            glm::mat4 modelMatrix = glm::translate(glm::mat4(1.f), tf.position) * 
                                    glm::mat4_cast(glm::quat(glm::radians(tf.rotation))) * 
                                    glm::scale(glm::mat4(1.f), tf.scale);
            renderCtx.PushVertexConstants(0, &modelMatrix, sizeof(glm::mat4));
            renderCtx.BindVertexBuffer(asset->GetVertexBuffer());
            renderCtx.BindIndexBuffer(asset->GetIndexBuffer());
            for (const auto& section : asset->GetSections()) {
                struct FragPC { uint32_t matIdx; uint32_t objID; } fpc;
                fpc.matIdx = section.materialIndex; fpc.objID = 0;
                renderCtx.PushFragmentConstants(0, &fpc, sizeof(FragPC));
                auto tex = AssetManager::GetFallbackTexture();
                renderCtx.BindFragmentTexture(0, tex.get());
                renderCtx.DrawIndexed(section.indexCount, 1, section.firstIndex);
            }
        }
    }, true);

    // Ghost/grid rendering temporarily disabled
}

void GameScene::LogicUpdate(SceneContext& ctx, float dt) {
    if (!ctx.network.IsConnected()) { ctx.scenes.RequestTransition(new MainMenuScene()); return; }
    if (Input::IsKeyPressed(SDLK_TAB)) {
        CameraMode oldMode = m_CameraMode;
        m_CameraMode = static_cast<CameraMode>((static_cast<int>(m_CameraMode) + 1) % 3);
        switch (m_CameraMode) {
        case CameraMode::Commander:
            if (oldMode != CameraMode::Commander) { m_SavedCamPos = m_Camera->m_Position; m_SavedCamYaw = m_Camera->m_Yaw; m_SavedCamPitch = m_Camera->m_Pitch; }
            m_PlacementActive = false;
            m_Camera->m_Pitch = -89.f; m_Camera->m_Yaw = -90.f;
            m_Camera->m_Position = glm::vec3(m_SavedCamPos.x, m_CmdHeight, m_SavedCamPos.z);
            Input::SetRelativeMouseMode(ctx.renderer->GetWindow()->handle, false);
            break;
        case CameraMode::Building:
            if (oldMode == CameraMode::Commander) { m_Camera->m_Position = m_SavedCamPos; m_Camera->m_Yaw = m_SavedCamYaw; m_Camera->m_Pitch = m_SavedCamPitch; }
            m_Camera->m_Pitch = -45.0f; m_Camera->m_Yaw = -135.0f;
            Input::SetRelativeMouseMode(ctx.renderer->GetWindow()->handle, false);
            break;
        case CameraMode::FreeFly:
            if (oldMode == CameraMode::Commander) { m_Camera->m_Position = m_SavedCamPos; m_Camera->m_Yaw = m_SavedCamYaw; m_Camera->m_Pitch = m_SavedCamPitch; }
            m_PlacementActive = false;
            Input::SetRelativeMouseMode(ctx.renderer->GetWindow()->handle, true);
            break;
        }
        m_Camera->UpdateVectors();
    }
    switch (m_CameraMode) {
    case CameraMode::Commander: {
        constexpr float CMD_PAN_SPEED = 25.f;
        glm::vec3 pan{0.f};
        if (Input::IsKeyDown(SDLK_W)) pan.z -= CMD_PAN_SPEED * dt;
        if (Input::IsKeyDown(SDLK_S)) pan.z += CMD_PAN_SPEED * dt;
        if (Input::IsKeyDown(SDLK_A)) pan.x -= CMD_PAN_SPEED * dt;
        if (Input::IsKeyDown(SDLK_D)) pan.x += CMD_PAN_SPEED * dt;
        m_Camera->m_Position += pan;
        m_CmdHeight -= Input::GetScrollDelta() * 2.f;
        m_CmdHeight = glm::clamp(m_CmdHeight, 5.f, 100.f);
        m_Camera->m_Position.y = m_CmdHeight;
        if (Input::IsMouseButtonPressed(SDL_BUTTON_LEFT) && !ImGui::GetIO().WantCaptureMouse) {
            glm::vec2 mp = Input::GetMousePosition();
            int winW, winH; SDL_GetWindowSize(ctx.renderer->GetWindow()->handle, &winW, &winH);
            glm::vec3 worldPos = ScreenToWorldXZ(mp.x, mp.y, winW, winH);
            m_SelectedUnits.clear();
            auto view = ctx.clientRegistry.view<TransformComponent, NetworkedComponent, UnitComponent>();
            for (auto entity : view) {
                auto& tf  = view.get<TransformComponent>(entity);
                auto& nc  = view.get<NetworkedComponent>(entity);
                auto& uc  = view.get<UnitComponent>(entity);
                if (uc.teamId != m_MyPlayerId % 2) continue;
                if (glm::length(glm::vec2(tf.position.x - worldPos.x, tf.position.z - worldPos.z)) <= 5.f) {
                    m_SelectedUnits.push_back(nc.netId);
                    uc.selected = true;
                } else uc.selected = false;
            }
        }
        if (Input::IsMouseButtonPressed(SDL_BUTTON_RIGHT) && !m_SelectedUnits.empty()) {
            glm::vec2 mp = Input::GetMousePosition();
            int winW, winH; SDL_GetWindowSize(ctx.renderer->GetWindow()->handle, &winW, &winH);
            glm::vec3 worldPos = ScreenToWorldXZ(mp.x, mp.y, winW, winH);
            CommanderOrderPacket pkt; pkt.playerId = m_MyPlayerId; pkt.x = worldPos.x; pkt.y = worldPos.y; pkt.z = worldPos.z;
            pkt.selectedCount = static_cast<uint32_t>(m_SelectedUnits.size());
            for (uint32_t i = 0; i < pkt.selectedCount && i < MAX_SELECTED_UNITS; ++i) pkt.selectedIds[i] = m_SelectedUnits[i];
            ctx.network.Send(pkt);
        }
        break;
    }
    case CameraMode::Building: {
        if (m_PlacementActive) {
            glm::vec2 mp = Input::GetMousePosition();
            int winW, winH; SDL_GetWindowSize(ctx.renderer->GetWindow()->handle, &winW, &winH);
            glm::vec3 worldPos = ScreenToWorldXZ(mp.x, mp.y, winW, winH);
            constexpr float GRID = 2.f;
            worldPos.x = std::round(worldPos.x / GRID) * GRID;
            worldPos.z = std::round(worldPos.z / GRID) * GRID;
            worldPos.y = 0.f; m_PlacementPos = worldPos;
            if (Input::IsMouseButtonPressed(SDL_BUTTON_LEFT) && !ImGui::GetIO().WantCaptureMouse) {
                BuildPlaceRequestPacket req; req.playerId = m_MyPlayerId; req.buildingType = static_cast<uint8_t>(m_PlacementType); req.x = worldPos.x; req.z = worldPos.z;
                ctx.network.Send(req);
            }
            if (Input::IsMouseButtonPressed(SDL_BUTTON_RIGHT) || Input::IsKeyPressed(SDLK_ESCAPE)) m_PlacementActive = false;
        }
        break;
    }
    case CameraMode::FreeFly: {
        if (Input::IsRelativeMouseMode()) {
            constexpr float FLY_SPEED = 10.f; glm::vec3 dir{0.f};
            if (Input::IsKeyDown(SDLK_W)) dir += m_Camera->m_Front;
            if (Input::IsKeyDown(SDLK_S)) dir -= m_Camera->m_Front;
            if (Input::IsKeyDown(SDLK_A)) dir -= m_Camera->m_Right;
            if (Input::IsKeyDown(SDLK_D)) dir += m_Camera->m_Right;
            if (glm::length(dir) > 0.1f) m_Camera->m_Position += glm::normalize(dir) * FLY_SPEED * dt;
            glm::vec2 md = Input::GetMouseDelta(); m_Camera->Rotate(md.x, md.y);
        }
        break;
    }
    }
    m_Camera->UpdateVectors();
    m_TotalTime += dt; m_FrameCount++;
}

void GameScene::UIUpdate(SceneContext& ctx, float dt) {
    if (m_GameOver) {
        ImGui::Begin("Spielende");
        ImGui::Text(m_WinnerTeam == m_MyPlayerId % 2 ? "SIEG!" : "NIEDERLAGE!");
        if (ImGui::Button("Hauptmenü")) { ctx.network.Disconnect(); ctx.scenes.RequestTransition(new MainMenuScene()); }
        ImGui::End(); return;
    }
    ImGui::Begin("Game Debug");
    ImGui::Text("FPS: %.1f", 1.0f / dt);
    const char* modeStrs[] = { "Commander", "Building", "FreeFly" };
    ImGui::Text("Kamera: %s", modeStrs[static_cast<int>(m_CameraMode)]);
    if (m_CameraMode == CameraMode::Building) {
        static const char* tNames[] = { "Attack", "Defense", "Resource", "Outpost" };
        static const BuildingType tVals[] = { BuildingType::Attack, BuildingType::Defense, BuildingType::Resource, BuildingType::Outpost };
        for (int i = 0; i < 4; ++i) if (ImGui::Selectable(tNames[i], m_PlacementActive && m_PlacementType == tVals[i])) { m_PlacementActive = true; m_PlacementType = tVals[i]; }
    }
    if (ImGui::Button("Disconnect")) { ctx.network.Disconnect(); ctx.scenes.RequestTransition(new MainMenuScene()); }
    ImGui::End();
    if (ctx.network.IsHosting()) ResourceHUD::Draw(ctx.serverRegistry, 0);
    if (m_CameraMode == CameraMode::Commander) DrawUnitHPBars(ctx);
}

void GameScene::SpawnPhysicsCube(SceneContext& ctx, glm::vec3 pos) {
    if (!ctx.network.IsHosting()) return;
    uint32_t netId = m_NextNetId++;
    auto e = ctx.serverRegistry.create();
    uint32_t physId = ctx.world->GetNextPhysicsID();
    auto h = ctx.physics->AddDynamicBox(physId, JPH::RVec3(pos.x, pos.y, pos.z), JPH::Vec3(1,1,1));
    ctx.world->RegisterPhysicsEntity(physId, e);
    ctx.serverRegistry.emplace<TransformComponent>(e, pos);
    ctx.serverRegistry.emplace<PhysicsBodyComponent>(e, h);
    ctx.serverRegistry.emplace<NetworkedComponent>(e, netId);
    ctx.serverRegistry.emplace<ModelComponent>(e, "assets/cube.glb");
    m_ServerNetMap[netId] = e;
    AssetJoinedPacket pkt; pkt.netId = netId; std::strncpy(pkt.modelPath, "assets/cube.glb", sizeof(pkt.modelPath)-1); pkt.x = pos.x; pkt.y = pos.y; pkt.z = pos.z;
    ctx.network.BroadcastToAll(pkt);
}

void GameScene::FixedUpdate(SceneContext& ctx, float dt) {
    if (ctx.network.IsHosting()) { PollConnectionEvents(ctx); PollClientPackets(ctx); }
    if (ctx.network.IsConnected()) PollServerPackets(ctx);
    if (ctx.network.IsHosting()) {
        Systems::MovementSystem(ctx.serverRegistry, dt);
        UpdateUnitMovement(ctx, dt); UpdateCombat(ctx, dt); CheckWinCondition(ctx);
        m_ResourceManager.Update(ctx.serverRegistry, dt);
        ResourceSystem::Update(ctx.serverRegistry, m_ResourceManager, dt);
        FogOfWarSystem::Update(m_Fog, ctx.serverRegistry);
        TerritorySystem::Update(ctx.serverRegistry, dt);
    }
    SendLocalInput(ctx);
    if (ctx.network.IsHosting()) { m_SnapAccum += dt; if (m_SnapAccum >= SNAPSHOT_RATE) { SendSnapshots(ctx); m_SnapAccum -= SNAPSHOT_RATE; } }
}

void GameScene::PollConnectionEvents(SceneContext& ctx) {
    uint32_t pid; bool conn;
    while (ctx.network.PollPlayerConnection(pid, conn)) {
        if (conn) {
            uint32_t nid = m_NextNetId++; uint32_t rpid = m_NextPlayerId++;
            for (auto& [id, ent] : m_ServerNetMap) {
                auto& t = ctx.serverRegistry.get<TransformComponent>(ent);
                if (auto* p = ctx.serverRegistry.try_get<PlayerComponent>(ent)) {
                    PlayerJoinedPacket pk; pk.netId = id; pk.playerId = p->playerId; pk.x = t.position.x; pk.y = t.position.y; pk.z = t.position.z;
                    ctx.network.SendToClient(pid, pk);
                }
                if (auto* m = ctx.serverRegistry.try_get<ModelComponent>(ent)) {
                    AssetJoinedPacket pk; pk.netId = id; std::strncpy(pk.modelPath, m->modelPath.c_str(), sizeof(pk.modelPath)-1); pk.x = t.position.x; pk.y = t.position.y; pk.z = t.position.z;
                    ctx.network.SendToClient(pid, pk);
                }
            }
            PlayerIdAssignPacket idpk; idpk.playerId = rpid; idpk.netId = nid; ctx.network.SendToClient(pid, idpk);
            auto e = ctx.serverRegistry.create(); ctx.serverRegistry.emplace<TransformComponent>(e); ctx.serverRegistry.emplace<MovementComponent>(e);
            ctx.serverRegistry.emplace<PlayerComponent>(e, rpid, false); ctx.serverRegistry.emplace<NetworkedComponent>(e, nid);
            m_ServerNetMap[nid] = e; m_PeerToNetId[pid] = nid;
            PlayerJoinedPacket bpk; bpk.netId = nid; bpk.playerId = rpid; ctx.network.BroadcastToAll(bpk);
        } else {
            auto it = m_PeerToNetId.find(pid);
            if (it != m_PeerToNetId.end()) {
                uint32_t nid = it->second; ctx.serverRegistry.destroy(m_ServerNetMap[nid]); m_ServerNetMap.erase(nid); m_PeerToNetId.erase(pid);
                PlayerLeftPacket pk; pk.netId = nid; ctx.network.BroadcastToAll(pk);
            }
        }
    }
}

void GameScene::PollClientPackets(SceneContext& ctx) {
    while (auto res = ctx.network.ReceiveFromClient<PlayerInputPacket>(PacketType::PLAYER_INPUT)) {
        auto [pk, spid] = *res; auto pit = m_PeerToNetId.find(spid); if (pit == m_PeerToNetId.end()) continue;
        auto eit = m_ServerNetMap.find(pit->second); if (eit == m_ServerNetMap.end()) continue;
        if (auto* mv = ctx.serverRegistry.try_get<MovementComponent>(eit->second)) mv->inputDir = {pk.dx, pk.dy, pk.dz};
        if (auto* tf = ctx.serverRegistry.try_get<TransformComponent>(eit->second)) { tf->rotation.y = pk.yaw; tf->rotation.x = pk.pitch; }
    }
    while (auto res = ctx.network.ReceiveFromClient<CommanderOrderPacket>(PacketType::COMMANDER_ORDER)) {
        auto [pk, spid] = *res; uint32_t team = pk.playerId % 2; glm::vec3 dest{pk.x, pk.y, pk.z};
        auto view = ctx.serverRegistry.view<UnitComponent, MovementOrderComponent>();
        for (auto e : view) { auto& uc = view.get<UnitComponent>(e); auto& mo = view.get<MovementOrderComponent>(e); if (uc.teamId == team && uc.selected) { mo.destination = dest; mo.active = true; } }
    }
    while (auto res = ctx.network.ReceiveFromClient<BuildPlaceRequestPacket>(PacketType::BUILD_PLACE_REQUEST)) {
        auto [pk, spid] = *res; glm::vec3 pos{pk.x, 0.f, pk.z};
        constexpr float SNAP = 2.f; float sx = std::round(pos.x/SNAP)*SNAP, sz = std::round(pos.z/SNAP)*SNAP; bool blk = false;
        auto bview = ctx.serverRegistry.view<TransformComponent, BuildingComponent>();
        for (auto ex : bview) { auto& et = bview.get<TransformComponent>(ex); if (std::abs(std::round(et.position.x/SNAP)*SNAP - sx) < 0.1f && std::abs(std::round(et.position.z/SNAP)*SNAP - sz) < 0.1f) { blk = true; break; } }
        if (blk) continue;
        uint32_t nid = m_NextNetId++; auto e = ctx.serverRegistry.create();
        ctx.serverRegistry.emplace<TransformComponent>(e, pos); ctx.serverRegistry.emplace<NetworkedComponent>(e, nid); ctx.serverRegistry.emplace<ModelComponent>(e, std::string("assets/cube.glb"));
        m_ServerNetMap[nid] = e; BuildingComponent bc; bc.type = static_cast<BuildingType>(pk.buildingType); bc.teamId = pk.playerId % 2; ctx.serverRegistry.emplace<BuildingComponent>(e, bc);
        AssetJoinedPacket aj; aj.netId = nid; std::strncpy(aj.modelPath, "assets/cube.glb", sizeof(aj.modelPath)-1); aj.x = pos.x; aj.y = pos.y; aj.z = pos.z; ctx.network.BroadcastToAll(aj);
    }
}

void GameScene::SendSnapshots(SceneContext& ctx) {
    auto view = ctx.serverRegistry.view<NetworkedComponent, TransformComponent>();
    for (auto e : view) {
        auto& n = view.get<NetworkedComponent>(e); auto& t = view.get<TransformComponent>(e);
        EntitySnapshotPacket pk; pk.netId = n.netId; pk.x = t.position.x; pk.y = t.position.y; pk.z = t.position.z;
        pk.rx = t.rotation.x; pk.ry = t.rotation.y; pk.rz = t.rotation.z; pk.sx = t.scale.x; pk.sy = t.scale.y; pk.sz = t.scale.z;
        if (auto* m = ctx.serverRegistry.try_get<MovementComponent>(e)) { pk.vx = m->velocity.x; pk.vy = m->velocity.y; pk.vz = m->velocity.z; } else pk.vx = pk.vy = pk.vz = 0.f;
        ctx.network.BroadcastToAll(pk);
    }
}

void GameScene::PollServerPackets(SceneContext& ctx) {
    if (!m_IdAssigned) if (auto pk = ctx.network.ReceiveFromServer<PlayerIdAssignPacket>(PacketType::PLAYER_ID_ASSIGN)) { m_MyPlayerId = pk->playerId; m_MyNetId = pk->netId; m_IdAssigned = true; }
    while (auto pk = ctx.network.ReceiveFromServer<PlayerJoinedPacket>(PacketType::PLAYER_JOINED)) {
        if (m_ClientNetMap.count(pk->netId)) continue; auto e = ctx.clientRegistry.create();
        ctx.clientRegistry.emplace<TransformComponent>(e, glm::vec3{pk->x, pk->y, pk->z}); ctx.clientRegistry.emplace<MovementComponent>(e);
        ctx.clientRegistry.emplace<PlayerComponent>(e, pk->playerId, pk->playerId == m_MyPlayerId); ctx.clientRegistry.emplace<NetworkedComponent>(e, pk->netId); m_ClientNetMap[pk->netId] = e;
    }
    while (auto pk = ctx.network.ReceiveFromServer<PlayerLeftPacket>(PacketType::PLAYER_LEFT)) { auto it = m_ClientNetMap.find(pk->netId); if (it != m_ClientNetMap.end()) { ctx.clientRegistry.destroy(it->second); m_ClientNetMap.erase(it); } }
    while (auto pk = ctx.network.ReceiveFromServer<EntitySnapshotPacket>(PacketType::ENTITY_SNAPSHOT)) {
        auto it = m_ClientNetMap.find(pk->netId); if (it == m_ClientNetMap.end() || ctx.network.IsHosting()) continue;
        if (auto* t = ctx.clientRegistry.try_get<TransformComponent>(it->second)) {
            bool local = false; if (auto* p = ctx.clientRegistry.try_get<PlayerComponent>(it->second)) local = p->isLocal;
            t->position = {pk->x, pk->y, pk->z}; if (!local) t->rotation = {pk->rx, pk->ry, pk->rz}; t->scale = {pk->sx, pk->sy, pk->sz};
        }
        if (auto* m = ctx.clientRegistry.try_get<MovementComponent>(it->second)) m->velocity = {pk->vx, pk->vy, pk->vz};
    }
    while (auto pk = ctx.network.ReceiveFromServer<AssetJoinedPacket>(PacketType::ASSET_JOINED)) {
        if (m_ClientNetMap.count(pk->netId)) continue; auto e = ctx.clientRegistry.create();
        ctx.clientRegistry.emplace<TransformComponent>(e, glm::vec3{pk->x, pk->y, pk->z}); ctx.clientRegistry.emplace<NetworkedComponent>(e, pk->netId); ctx.clientRegistry.emplace<ModelComponent>(e, std::string(pk->modelPath)); m_ClientNetMap[pk->netId] = e;
    }
    while (auto pk = ctx.network.ReceiveFromServer<UnitSpawnedPacket>(PacketType::UNIT_SPAWNED)) {
        if (m_ClientNetMap.count(pk->netId)) continue; auto e = ctx.clientRegistry.create();
        ctx.clientRegistry.emplace<TransformComponent>(e, glm::vec3{pk->x, pk->y, pk->z}); ctx.clientRegistry.emplace<MovementComponent>(e); ctx.clientRegistry.emplace<NetworkedComponent>(e, pk->netId); ctx.clientRegistry.emplace<ModelComponent>(e, std::string("assets/cube.glb"));
        ctx.clientRegistry.emplace<UnitComponent>(e, UnitComponent{pk->teamId, static_cast<BugClass>(pk->bugClass), false}); ctx.clientRegistry.emplace<HealthComponent>(e, HealthComponent{pk->maxHp}); ctx.clientRegistry.emplace<MovementOrderComponent>(e); m_ClientNetMap[pk->netId] = e;
    }
    while (auto pk = ctx.network.ReceiveFromServer<UnitDiedPacket>(PacketType::UNIT_DIED)) {
        auto it = m_ClientNetMap.find(pk->netId); if (it == m_ClientNetMap.end()) continue; ctx.clientRegistry.destroy(it->second); m_ClientNetMap.erase(it);
        m_SelectedUnits.erase(std::remove(m_SelectedUnits.begin(), m_SelectedUnits.end(), pk->netId), m_SelectedUnits.end());
    }
    while (auto pk = ctx.network.ReceiveFromServer<UnitHpUpdatePacket>(PacketType::UNIT_HP_UPDATE)) {
        auto it = m_ClientNetMap.find(pk->netId); if (it == m_ClientNetMap.end()) continue; if (auto* hc = ctx.clientRegistry.try_get<HealthComponent>(it->second)) hc->hp = pk->hp;
    }
    if (auto pk = ctx.network.ReceiveFromServer<GameOverPacket>(PacketType::GAME_OVER)) { m_GameOver = true; m_WinnerTeam = pk->winnerTeam; }
}

void GameScene::SendLocalInput(SceneContext& ctx) {
    if (!ctx.network.IsConnected() || !Input::IsRelativeMouseMode()) return;
    glm::vec3 fwd = m_Camera->m_Front; fwd.y = 0.f; if (glm::length(fwd) > 0.0001f) fwd = glm::normalize(fwd);
    glm::vec3 rgt = m_Camera->m_Right; rgt.y = 0.f; if (glm::length(rgt) > 0.0001f) rgt = glm::normalize(rgt);
    glm::vec3 dir{0.f}; if (Input::IsKeyDown(SDLK_W)) dir += fwd; if (Input::IsKeyDown(SDLK_S)) dir -= fwd; if (Input::IsKeyDown(SDLK_A)) dir -= rgt; if (Input::IsKeyDown(SDLK_D)) dir += rgt;
    if (Input::IsKeyDown(SDLK_SPACE)) dir.y += 1.f; if (Input::IsKeyDown(SDLK_LSHIFT)) dir.y -= 1.f;
    if (glm::length(dir) > 0.f) dir = glm::normalize(dir);
    PlayerInputPacket pk; pk.dx = dir.x; pk.dy = dir.y; pk.dz = dir.z; pk.yaw = m_Camera->m_Yaw; pk.pitch = m_Camera->m_Pitch; ctx.network.Send(pk);
}

void GameScene::LoadSceneMeshCollision(SceneContext& ctx, const std::string& glbPath, const glm::mat4& transform) {
    if (!ctx.physics) return; SceneData sd = AssetManager::LoadGLTF(glbPath); if (!sd.model) return;
    if (sd.cpuVertices.empty() || sd.cpuIndices.empty()) return;
    JPH::Shape::ShapeResult res = MeshCollisionBuilder::Build(sd.cpuVertices, sd.cpuIndices, transform);
    if (!res.IsValid()) return;
    PhysicsBodyHandle h = ctx.physics->AddStaticMesh(res.Get(), JPH::RVec3::sZero(), JPH::Quat::sIdentity());
    if (h.IsValid()) m_MeshCollisionBodies.push_back(h);
}

void GameScene::SpawnUnit(SceneContext& ctx, uint32_t teamId, glm::vec3 pos, float hp) {
    if (!ctx.network.IsHosting()) return;
    static std::mt19937 rng{std::random_device{}()};
    static const BugClass clss[] = { BugClass::Ants, BugClass::Beetles, BugClass::Mantis, BugClass::Dragonflies, BugClass::Roaches, BugClass::Scorpions };
    BugClass bc = clss[rng() % std::size(clss)];
    float dmg = 7.f; if (bc == BugClass::Mantis || bc == BugClass::Dragonflies || bc == BugClass::Scorpions) dmg = 20.f; else if (bc == BugClass::Ants || bc == BugClass::Roaches || bc == BugClass::Beetles || bc == BugClass::CentipedesWorms) dmg = 12.f;
    uint32_t nid = m_NextNetId++; auto e = ctx.serverRegistry.create();
    ctx.serverRegistry.emplace<TransformComponent>(e, pos); ctx.serverRegistry.emplace<MovementComponent>(e); ctx.serverRegistry.emplace<NetworkedComponent>(e, nid); ctx.serverRegistry.emplace<ModelComponent>(e, std::string("assets/cube.glb"));
    ctx.serverRegistry.emplace<UnitComponent>(e, UnitComponent{teamId, bc, false}); ctx.serverRegistry.emplace<HealthComponent>(e, HealthComponent{hp});
    ctx.serverRegistry.emplace<CombatComponent>(e, CombatComponent{6.f, dmg, 0.f, 1.5f, entt::null}); ctx.serverRegistry.emplace<MovementOrderComponent>(e); m_ServerNetMap[nid] = e;
    UnitSpawnedPacket pk; pk.netId = nid; pk.teamId = teamId; pk.bugClass = static_cast<uint8_t>(bc); pk.x = pos.x; pk.y = pos.y; pk.z = pos.z; pk.hp = hp; pk.maxHp = hp; ctx.network.BroadcastToAll(pk);
}

glm::vec3 GameScene::RandomSpawnInTerritory(SceneContext& ctx, uint32_t teamId) {
    struct ZI { glm::vec3 c; float hw, hd; }; std::vector<ZI> cand;
    auto view = ctx.serverRegistry.view<TransformComponent, TerritoryComponent>(); uint32_t idx = 0;
    for (auto e : view) { const auto& tf = view.get<TransformComponent>(e); const auto& tr = view.get<TerritoryComponent>(e); if (idx % 2 == teamId % 2) cand.push_back({tf.position, tr.halfW, tr.halfD}); idx++; }
    if (cand.empty()) return {(teamId == 0) ? -20.f : 20.f, 0.f, 0.f};
    static std::mt19937 rng{std::random_device{}()}; const auto& z = cand[rng() % cand.size()];
    std::uniform_real_distribution<float> rx(-z.hw, z.hw), rz(-z.hd, z.hd); return {z.c.x + rx(rng), 0.f, z.c.z + rz(rng)};
}

void GameScene::UpdateUnitMovement(SceneContext& ctx, float dt) {
    auto view = ctx.serverRegistry.view<TransformComponent, MovementComponent, MovementOrderComponent>();
    for (auto e : view) {
        auto& tf = view.get<TransformComponent>(e); auto& mv = view.get<MovementComponent>(e); auto& mo = view.get<MovementOrderComponent>(e);
        if (!mo.active) { mv.velocity = {0,0,0}; continue; }
        glm::vec3 dir = mo.destination - tf.position; dir.y = 0.f; float dist = glm::length(dir);
        if (dist < 1.f) { mo.active = false; mv.velocity = {0,0,0}; } else { dir = glm::normalize(dir); mv.velocity = dir * mv.speed; tf.position += mv.velocity * dt; tf.rotation.y = glm::degrees(std::atan2(dir.x, dir.z)); }
    }
}

void GameScene::UpdateCombat(SceneContext& ctx, float dt) {
    struct UI { entt::entity e; glm::vec3 p; uint32_t t; float h; }; std::vector<UI> uis;
    { auto view = ctx.serverRegistry.view<TransformComponent, UnitComponent, HealthComponent>(); for (auto e : view) { auto& tc = view.get<TransformComponent>(e); auto& uc = view.get<UnitComponent>(e); auto& hc = view.get<HealthComponent>(e); if (!hc.dead) uis.push_back({e, tc.position, uc.teamId, hc.hp}); } }
    auto view = ctx.serverRegistry.view<TransformComponent, UnitComponent, HealthComponent, CombatComponent, MovementOrderComponent>();
    std::vector<std::pair<entt::entity, uint32_t>> tk;
    for (auto e : view) {
        auto& tf = view.get<TransformComponent>(e); auto& uc = view.get<UnitComponent>(e); auto& hc = view.get<HealthComponent>(e); auto& cc = view.get<CombatComponent>(e); auto& mo = view.get<MovementOrderComponent>(e);
        if (hc.dead) continue; if (cc.attackCooldown > 0.f) cc.attackCooldown -= dt;
        bool valid = (cc.target != entt::null) && ctx.serverRegistry.valid(cc.target);
        if (valid) { if (auto* thc = ctx.serverRegistry.try_get<HealthComponent>(cc.target)) { if (thc->dead) { valid = false; cc.target = entt::null; } } else { valid = false; cc.target = entt::null; } }
        if (!valid) { float bd = cc.attackRange; for (const auto& i : uis) { if (i.t == uc.teamId) continue; float d = glm::length(i.p - tf.position); if (d < bd) { bd = d; cc.target = i.e; valid = true; } } }
        if (!valid) continue;
        if (auto* ttf = ctx.serverRegistry.try_get<TransformComponent>(cc.target)) {
            float dist = glm::length(ttf->position - tf.position);
            if (dist <= cc.attackRange) {
                mo.active = false;
                if (cc.attackCooldown <= 0.f) {
                    cc.attackCooldown = cc.attackRate;
                    if (auto* thc = ctx.serverRegistry.try_get<HealthComponent>(cc.target)) {
                        thc->hp -= cc.attackDamage;
                        if (auto* tnc = ctx.serverRegistry.try_get<NetworkedComponent>(cc.target)) { UnitHpUpdatePacket hpk; hpk.netId = tnc->netId; hpk.hp = thc->hp; ctx.network.BroadcastToAll(hpk); }
                        if (thc->hp <= 0.f) { thc->dead = true; if (auto* dnc = ctx.serverRegistry.try_get<NetworkedComponent>(cc.target)) tk.push_back({cc.target, dnc->netId}); cc.target = entt::null; }
                    }
                }
            }
        } else cc.target = entt::null;
    }
    auto bview = ctx.serverRegistry.view<TransformComponent, BaseHealthComponent>();
    for (auto& i : uis) {
        if (auto* cc2 = ctx.serverRegistry.try_get<CombatComponent>(i.e)) {
            if (cc2->target != entt::null) continue;
            float bd = cc2->attackRange; entt::entity bb = entt::null;
            for (auto be : bview) { auto& bt = bview.get<TransformComponent>(be); auto& bh = bview.get<BaseHealthComponent>(be); if (bh.teamId != i.t && !bh.destroyed) { float d = glm::length(bt.position - i.p); if (d < bd) { bd = d; bb = be; } } }
            if (bb != entt::null && cc2->attackCooldown <= 0.f) { cc2->attackCooldown = cc2->attackRate; auto& bh = bview.get<BaseHealthComponent>(bb); bh.hp -= cc2->attackDamage; if (bh.hp <= 0.f) { bh.hp = 0.f; bh.destroyed = true; } }
        }
    }
    for (auto& [de, dn] : tk) HandleUnitDeath(ctx, de, dn);
}

void GameScene::HandleUnitDeath(SceneContext& ctx, entt::entity e, uint32_t nid) {
    if (!ctx.serverRegistry.valid(e)) return;
    if (auto* tf = ctx.serverRegistry.try_get<TransformComponent>(e)) m_ResourceManager.SpawnMeatDrop(ctx.serverRegistry, tf->position, 1, 20.f);
    UnitDiedPacket pk; pk.netId = nid; ctx.network.BroadcastToAll(pk); m_ServerNetMap.erase(nid); ctx.serverRegistry.destroy(e);
}

void GameScene::CheckWinCondition(SceneContext& ctx) {
    if (m_GameOver) return; std::unordered_map<uint32_t, int> surv; auto view = ctx.serverRegistry.view<BaseHealthComponent>();
    for (auto e : view) { auto& bh = view.get<BaseHealthComponent>(e); if (!bh.destroyed) surv[bh.teamId]++; }
    if (surv.empty()) return; std::vector<uint32_t> elim; for (int t = 0; t < 2; t++) if (surv[t] == 0) elim.push_back(t);
    if (elim.empty()) return; uint32_t win = 0xFFFFFFFFu; for (auto& [t, c] : surv) if (c > 0) { win = t; break; }
    m_GameOver = true; m_WinnerTeam = win; GameOverPacket pk; pk.winnerTeam = win; ctx.network.BroadcastToAll(pk);
}

glm::vec3 GameScene::ScreenToWorldXZ(float sx, float sy, int winW, int winH) {
    float nx = (2.f * sx / (float)winW) - 1.f, ny = 1.f - (2.f * sy / (float)winH);
    float asp = (winW > 0) ? (float)winW / (float)winH : 1.f;
    glm::mat4 inv = glm::inverse(m_Camera->GetProjectionMatrix(asp) * m_Camera->GetViewMatrix());
    glm::vec4 n = inv * glm::vec4(nx, ny, -1.f, 1.f), f = inv * glm::vec4(nx, ny, 1.f, 1.f);
    n /= n.w; f /= f.w; glm::vec3 ro{n}, rd = glm::normalize(glm::vec3(f) - ro);
    if (std::abs(rd.y) < 1e-6f) return {0,0,0}; float t = -ro.y / rd.y; return ro + t * rd;
}

void GameScene::DrawUnitHPBars(SceneContext& ctx) {
    int w, h; SDL_GetWindowSizeInPixels(ctx.renderer->GetWindow()->handle, &w, &h); if (w <= 0 || h <= 0) return;
    glm::mat4 vp = m_Camera->GetProjectionMatrix((float)w/(float)h) * m_Camera->GetViewMatrix();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    auto view = ctx.clientRegistry.view<TransformComponent, HealthComponent, UnitComponent, NetworkedComponent>();
    for (auto e : view) {
        auto& tf = view.get<TransformComponent>(e); auto& hc = view.get<HealthComponent>(e); auto& uc = view.get<UnitComponent>(e); auto& nc = view.get<NetworkedComponent>(e);
        glm::vec4 clp = vp * glm::vec4(tf.position + glm::vec3(0, 2.5f, 0), 1.f); if (clp.w <= 0.f) continue; clp /= clp.w;
        if (clp.x < -1.f || clp.x > 1.f || clp.y < -1.f || clp.y > 1.f) continue;
        float sx = (clp.x * 0.5f + 0.5f) * (float)w, sy = (1.f - (clp.y * 0.5f + 0.5f)) * (float)h;
        dl->AddRectFilled({sx-20, sy}, {sx+20, sy+5}, IM_COL32(30,30,30,200));
        float frc = (hc.maxHp > 0.f) ? glm::clamp(hc.hp/hc.maxHp, 0.f, 1.f) : 0.f;
        dl->AddRectFilled({sx-20, sy}, {sx-20+40*frc, sy+5}, (uc.teamId == 0) ? IM_COL32(60,140,255,220) : IM_COL32(255,60,60,220));
        if (std::find(m_SelectedUnits.begin(), m_SelectedUnits.end(), nc.netId) != m_SelectedUnits.end()) dl->AddRect({sx-21, sy-1}, {sx+21, sy+6}, IM_COL32(255,255,0,255), 0, 0, 1.5f);
    }
}
