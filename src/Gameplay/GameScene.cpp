/**
 * @file GameScene.cpp
 * @brief Implementation of the main gameplay scene.
 */

#define GLM_ENABLE_EXPERIMENTAL
#include "GameScene.h"
#include "MainMenuScene.h"
#include "Components.h"
#include "Systems.h"
#include "../Networking/NetworkManager.h"
#include "../Networking/Packets.h"
#include "../Core/Input.h"
#include "../Core/AssetManager.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/API/Shader.h"
#include "../Graphics/API/GraphicsPipeline.h"
#include "../Graphics/API/Framebuffer.h"
#include "../Graphics/API/GPUBuffer.h"
#include "../Graphics/Lights.h"     // LightComponent + Light
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>
#include <cstring>

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

    if (!m_Camera) {
        m_Camera = std::make_unique<Camera>();
        m_Camera->m_Position = {0, 2, 10};
        m_Camera->m_Yaw = -90.0f;
        m_Camera->m_Pitch = 0.0f;
        m_Camera->UpdateVectors();
    }
    // Licht-Werte werden direkt aus den Klassen-Membern in GameScene.h übernommen.
    // Die dortigen Standardwerte (m_SunDirection, m_SunIntensity, m_AmbientColor usw.)
    // sind bereits gut abgestimmt — hier keine doppelte Initialisierung nötig.

    if (ctx.network.IsHosting()) {
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
    }
}

/**
 * @brief Clears registries and resets network/rendering state on exit.
 * @param ctx The scene context.
 */
void GameScene::OnExit(SceneContext& ctx) {
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

    m_VertShader.reset();
    m_FragShader.reset();
    m_ModelPipeline = nullptr;

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
 * @brief Renders the game world — models, sun, and all ECS light entities.
 * @param ctx The scene context.
 * @param renderer Pointer to the renderer.
 */
void GameScene::Render(SceneContext& ctx, Renderer* renderer) {

    // ------------------------------------------------------------------
    // 1. Lazy-init graphics pipelines
    // ------------------------------------------------------------------
    if (!m_ModelPipeline) {
        // Model pass: 1 vert storage buf + 1 vert UB | 2 samplers + 2 frag storage bufs + 1 frag UB
        ShaderResourceLayout vertLayout = {0, 0, 1, 1};
        ShaderResourceLayout fragLayout = {2, 0, 2, 1};

        m_VertShader = std::make_unique<Shader>(
            renderer->GetDevice(), "shaders/model.vert.spv",
            ShaderStage::Vertex, vertLayout);
        m_FragShader = std::make_unique<Shader>(
            renderer->GetDevice(), "shaders/model.frag.spv",
            ShaderStage::Fragment, fragLayout);

        PipelineConfig config;
        config.vertexShader   = m_VertShader.get();
        config.fragmentShader = m_FragShader.get();
        config.vertexStride   = sizeof(ModelVertex);
        config.vertexAttributes = {
            {0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(ModelVertex, position)},
            {1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(ModelVertex, normal)},
            {2, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(ModelVertex, texCoords)},
            {3, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(ModelVertex, color)},
            {4, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(ModelVertex, tangent)}
        };
        config.enableDepthTest  = true;
        config.depthCompareOp   = SDL_GPU_COMPAREOP_GREATER; // Reverse-Z

        config.colorTargetFormats = {
            SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
            SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
            SDL_GPU_TEXTUREFORMAT_R16_UINT
        };

        m_ModelPipeline = renderer->GetPipelines()->CreatePipeline(
            "GameModelPipeline", config, SDL_GPU_TEXTUREFORMAT_INVALID);
    }

    if (!m_ShadowPipeline ||
        m_ShadowBiasConstant != m_LastShadowBiasConstant ||
        m_ShadowBiasSlope    != m_LastShadowBiasSlope) {
        // Reset so the pipeline block below always recreates it cleanly
        m_ShadowPipeline = nullptr;
        m_LastShadowBiasConstant = m_ShadowBiasConstant;
        m_LastShadowBiasSlope    = m_ShadowBiasSlope;
        // Shadow pass: depth-only, 0 vert storage bufs + 2 vert UBs (model + sunVP), no frag resources
        ShaderResourceLayout shadowVertLayout = {0, 0, 0, 2};
        ShaderResourceLayout shadowFragLayout = {0, 0, 0, 0};

        m_ShadowVertShader = std::make_unique<Shader>(
            renderer->GetDevice(), "shaders/shadow.vert.spv",
            ShaderStage::Vertex, shadowVertLayout);
        m_ShadowFragShader = std::make_unique<Shader>(
            renderer->GetDevice(), "shaders/shadow.frag.spv",
            ShaderStage::Fragment, shadowFragLayout);

        PipelineConfig shadowCfg;
        shadowCfg.vertexShader   = m_ShadowVertShader.get();
        shadowCfg.fragmentShader = m_ShadowFragShader.get();
        shadowCfg.vertexStride   = sizeof(ModelVertex);
        shadowCfg.vertexAttributes = {
            {0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(ModelVertex, position)}
        };
        shadowCfg.enableDepthTest = true;
        shadowCfg.depthCompareOp  = SDL_GPU_COMPAREOP_LESS;
        shadowCfg.cullMode        = SDL_GPU_CULLMODE_FRONT; // Cull front faces: eliminates self-shadowing acne

        // GPU-side polygon offset — the correct place to fight shadow acne.
        // Values driven by m_ShadowBiasConstant/Slope, adjustable via ImGui at runtime.
        shadowCfg.enableDepthBias         = true;
        shadowCfg.depthBiasConstantFactor = m_ShadowBiasConstant;
        shadowCfg.depthBiasSlopeFactor    = m_ShadowBiasSlope;
        shadowCfg.depthBiasClamp          = 0.0f;
        // No color targets — depth only
        shadowCfg.colorTargetFormats = {};

        m_ShadowPipeline = renderer->GetPipelines()->CreatePipeline(
            "ShadowPipeline", shadowCfg, SDL_GPU_TEXTUREFORMAT_INVALID);

        // sunVP is now pushed as a vertex uniform constant (slot 1) — no dedicated buffer needed.
    }

    // ------------------------------------------------------------------
    // 2. Camera matrices
    // ------------------------------------------------------------------
    int w, h;
    SDL_GetWindowSizeInPixels(renderer->GetWindow()->handle, &w, &h);
    float aspect = (float)w / (float)h;

    GlobalUniforms& globals = renderer->GetGlobalUniforms();
    globals.view     = m_Camera->GetViewMatrix();
    globals.proj     = m_Camera->GetProjectionMatrix(aspect);
    globals.viewProj = globals.proj * globals.view;
    globals.cameraPos = glm::vec4(m_Camera->m_Position, 1.0f);

    // ------------------------------------------------------------------
    // 3. Sun light + sunVP matrix for shadow mapping
    // ------------------------------------------------------------------
    globals.sunDir   = glm::vec4(glm::normalize(-m_SunDirection), 0.0f);
    globals.sunColor = glm::vec4(m_SunColor, m_SunIntensity);

    // Build an orthographic projection from the sun's point of view.
    // The frustum is centered on the camera position so shadows follow the player
    // and there is no "ring" artifact at the frustum boundary.
    {
        const float shadowOrthoSize = m_ShadowOrthoSize;
        const float shadowNear      = 0.1f;
        const float shadowFar       = 200.0f;


        glm::vec3 camPos = m_Camera->m_Position;
        glm::vec3 sunDir = glm::normalize(m_SunDirection);

        // ✅ UP ZUERST!
        glm::vec3 up = (glm::abs(sunDir.y) < 0.99f)
            ? glm::vec3(0, 1, 0)
            : glm::vec3(1, 0, 0); // Avoid gimbal lock

        // Mittelpunkt = fester Weltmittelpunkt (Option A)
        glm::vec3 center = glm::vec3(0.0f, 0.0f, 0.0f);




        // Lichtposition entlang Richtung
        glm::vec3 sunPos = center - sunDir * 100.0f;

        // Blickrichtung entlang Licht (WICHTIG!)
        glm::vec3 target = center + sunDir;

        // ✅ Jetzt korrekt
        glm::mat4 sunView = glm::lookAt(sunPos, target, up);

        float size = shadowOrthoSize;

        glm::mat4 sunProj = glm::ortho(
            -size, size,
            -size, size,
            shadowNear, shadowFar  // Forward-Z: near=0.1, far=200
        );

        // --- Texel Snapping ---
        // Snap in light-view space (BEFORE projection) to prevent shadow swimming.
        // The previous approach snapped in clip-space after projection, which is
        // mathematically wrong and caused the shadow pattern to shift with camera movement.
        //
        // Correct approach:
        // 1. Transform world origin into light-view space.
        // 2. Snap XY to nearest texel boundary in world-unit scale.
        // 3. Reconstruct sunPos from the snapped offset so the view matrix is stable.
        float texelSize = (2.0f * shadowOrthoSize) / 2048.0f; // world units per shadow texel

        glm::vec4 originLV = sunView * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        glm::vec2 snappedXY = glm::round(glm::vec2(originLV) / texelSize) * texelSize;
        glm::vec2 snapDelta = snappedXY - glm::vec2(originLV);

        // Extract light-space axes from the view matrix rows
        glm::vec3 lightRight = glm::vec3(sunView[0][0], sunView[1][0], sunView[2][0]);
        glm::vec3 lightUp    = glm::vec3(sunView[0][1], sunView[1][1], sunView[2][1]);
        sunPos += lightRight * snapDelta.x + lightUp * snapDelta.y;
        sunView = glm::lookAt(sunPos, sunPos + sunDir, up);

        globals.sunVP = sunProj * sunView;
    }

    // sunVP is pushed as a vertex uniform constant directly in the shadow pass lambda.

    // ------------------------------------------------------------------
    // 4. Ambient light
    // ------------------------------------------------------------------
    globals.ambientColor = glm::vec4(m_AmbientColor, m_AmbientIntensity);

    // ------------------------------------------------------------------
    // 5. Dynamic lights — from ECS LightComponents + GLB-embedded lights
    // ------------------------------------------------------------------
    uint32_t totalLightCount = 0;

    // 5a. Lights from ECS entities with LightComponent + TransformComponent
    {
        auto lightView = ctx.clientRegistry.view<TransformComponent, LightComponent>();
        for (auto entity : lightView) {
            if (totalLightCount >= 16) break;

            auto& tf  = lightView.get<TransformComponent>(entity);
            auto& lc  = lightView.get<LightComponent>(entity);

            Light& out = globals.lights[totalLightCount];
            out.position_type   = glm::vec4(tf.position, (float)lc.type);
            out.direction_range = glm::vec4(glm::normalize(lc.direction), lc.range);
            out.color_intensity = glm::vec4(lc.color, lc.intensity);
            totalLightCount++;
        }
    }

    // 5b. Lights embedded inside GLB/GLTF model files
    {
        auto modelView = ctx.clientRegistry.view<TransformComponent, ModelComponent>();
        for (auto entity : modelView) {
            if (totalLightCount >= 16) break;

            auto& transform = modelView.get<TransformComponent>(entity);
            auto& modelComp = modelView.get<ModelComponent>(entity);
            auto sceneData  = AssetManager::LoadGLTF(modelComp.modelPath);
            if (!sceneData.model) {
                spdlog::warn("GameScene::Render — LoadGLTF fehlgeschlagen: {}", modelComp.modelPath);
                continue;
            }

            glm::mat4 entityMat =
                glm::translate(glm::mat4(1.0f), transform.position) *
                glm::mat4_cast(glm::quat(glm::radians(transform.rotation))) *
                glm::scale(glm::mat4(1.0f), transform.scale);

            for (const auto& light : sceneData.lights) {
                if (totalLightCount >= 16) break;

                Light& outLight = globals.lights[totalLightCount];
                outLight = light;

                // Transform position to world space
                glm::vec4 worldPos = entityMat * glm::vec4(glm::vec3(light.position_type), 1.0f);
                outLight.position_type = glm::vec4(glm::vec3(worldPos), light.position_type.w);

                // Transform direction to world space (directional / spot)
                if ((int)light.position_type.w == (int)LightType::Directional ||
                    (int)light.position_type.w == (int)LightType::Spot)
                {
                    glm::vec4 worldDir = entityMat * glm::vec4(glm::vec3(light.direction_range), 0.0f);
                    outLight.direction_range = glm::vec4(
                        glm::normalize(glm::vec3(worldDir)), light.direction_range.w);
                }
                totalLightCount++;
            }
        }
    }

    globals.timers = glm::vec4(m_TotalTime, (float)totalLightCount, 0.016f, (float)m_FrameCount);
    renderer->UpdateGlobalUniforms(globals);

    // ------------------------------------------------------------------
    // 6. Shadow Pass — render scene depth from sun's POV
    // ------------------------------------------------------------------
    constexpr uint32_t SHADOW_MAP_SIZE = 2048;

    if (!m_ShadowMap) {
        // Depth-only framebuffer with compare sampler for sampler2DShadow
        m_ShadowMap = std::make_unique<Framebuffer>(
            renderer->GetDevice(), SHADOW_MAP_SIZE, SHADOW_MAP_SIZE,
            std::vector<SDL_GPUTextureFormat>{}, // No color targets
            true,
            TextureFilter::ShadowCompare);
        spdlog::info("GameScene: Shadow map created ({}x{})", SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
    }

    // Copy sunVP before the lambda — globals is a local reference and cannot be captured.
    glm::mat4 sunVP = globals.sunVP;

    renderer->AddPass("ShadowPass", m_ShadowMap.get(),
        [this, ctx, renderer, sunVP](RenderContext& renderCtx)
    {
        if (!m_ShadowPipeline) return;

        renderCtx.BindPipeline(m_ShadowPipeline);
        // Push sunVP as vertex uniform slot 1 (slot 0 = model matrix, pushed per-instance below)
        renderCtx.PushVertexConstants(1, &sunVP, sizeof(glm::mat4));

        auto view = ctx.clientRegistry.view<TransformComponent, ModelComponent>();
        for (auto entity : view) {
            auto& transform = view.get<TransformComponent>(entity);
            auto& modelComp = view.get<ModelComponent>(entity);

            auto sceneData = AssetManager::LoadGLTF(modelComp.modelPath);
            if (!sceneData.model) continue;

            renderCtx.BindVertexBuffer(sceneData.model->GetVertexBuffer());
            renderCtx.BindIndexBuffer(sceneData.model->GetIndexBuffer());

            for (const auto& instance : sceneData.meshInstances) {
                struct ShadowPC { glm::mat4 model; } spc;
                glm::mat4 entityMat =
                    glm::translate(glm::mat4(1.0f), transform.position) *
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
    }, true, nullptr, 1.0f); // Forward-Z shadow map: 1.0 = Far

    // ------------------------------------------------------------------
    // 7. Render pass — draw all model entities into the G-Buffer
    // ------------------------------------------------------------------
    Framebuffer* gbuffer = renderer->GetGBuffer();
    if (!gbuffer) return;

    renderer->AddPass("GameRenderPass", gbuffer,
        [this, ctx, renderer](RenderContext& renderCtx)
    {
        renderCtx.BindPipeline(m_ModelPipeline);
        renderCtx.BindVertexStorageBuffer(0, renderer->GetGlobalUBO());
        renderCtx.BindFragmentStorageBuffer(0, renderer->GetGlobalUBO());

        // Bind shadow map to sampler slot 1 (slot 0 = base color texture, bound per-mesh below)
        if (m_ShadowMap && m_ShadowMap->GetDepthTarget())
            renderCtx.BindFragmentTexture(1, m_ShadowMap->GetDepthTarget());

        auto view = ctx.clientRegistry.view<TransformComponent, ModelComponent>();
        for (auto entity : view) {
            auto& transform = view.get<TransformComponent>(entity);
            auto& modelComp = view.get<ModelComponent>(entity);

            auto sceneData = AssetManager::LoadGLTF(modelComp.modelPath);
            if (!sceneData.model) {
                spdlog::warn("GameScene::Render — LoadGLTF fehlgeschlagen: {}", modelComp.modelPath);
            continue;
            }

            renderCtx.BindVertexBuffer(sceneData.model->GetVertexBuffer());
            renderCtx.BindIndexBuffer(sceneData.model->GetIndexBuffer());

            if (sceneData.model->GetMaterialBuffer())
                renderCtx.BindFragmentStorageBuffer(1, sceneData.model->GetMaterialBuffer());

            const auto& allSections  = sceneData.model->GetSections();
            const auto& allMaterials = sceneData.model->GetMaterials();

            for (const auto& instance : sceneData.meshInstances) {
                struct ModelPC { glm::mat4 model; } modelPC;
                glm::mat4 entityMat =
                    glm::translate(glm::mat4(1.0f), transform.position) *
                    glm::mat4_cast(glm::quat(glm::radians(transform.rotation))) *
                    glm::scale(glm::mat4(1.0f), transform.scale);
                modelPC.model = entityMat * instance.transform;
                renderCtx.PushVertexConstants(0, &modelPC, sizeof(ModelPC));

                for (uint32_t i = 0; i < instance.sectionCount; ++i) {
                    const auto& section = allSections[instance.firstSection + i];
                    struct FragPC { uint32_t matIdx; uint32_t objID; uint32_t pad1; uint32_t pad2; } fpc;
                    fpc.matIdx = (uint32_t)section.materialIndex;
                    fpc.objID  = 0;
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
    }, true);
}

// ---------------------------------------------------------------------------
// Per-frame update
// ---------------------------------------------------------------------------

/**
 * @brief Handles per-frame logic such as input processing, camera movement, and ImGui.
 * @param ctx The scene context.
 * @param dt Delta time.
 */
void GameScene::FrameUpdate(SceneContext& ctx, float dt) {
    if (!ctx.network.IsConnected()) {
        ctx.scenes.RequestTransition(new MainMenuScene());
        return;
    }

    // F1 toggles between Player and Free Fly
    if (Input::IsKeyPressed(SDLK_F1)) {
        m_FreeFly = !m_FreeFly;
        spdlog::info("Control Mode: {}", m_FreeFly ? "Free Fly" : "Player");
        if (m_FreeFly) Input::SetRelativeMouseMode(ctx.renderer->GetWindow()->handle, true);
    }

    // F2 toggles mouse capture independently
    if (Input::IsKeyPressed(SDLK_F2)) {
        bool newState = !Input::IsRelativeMouseMode();
        Input::SetRelativeMouseMode(ctx.renderer->GetWindow()->handle, newState);
        spdlog::info("Mouse Capture: {}", newState ? "On" : "Off");
    }

    /**
     * @brief Process camera rotation and movement based on input mode.
     */
    if (Input::IsRelativeMouseMode()) {
        glm::vec2 delta = Input::GetMouseDelta();
        m_Camera->Rotate(delta.x, delta.y);

        if (m_FreeFly) {
            // --- FREE FLY MODE (Captured) ---
            if (Input::IsKeyDown(SDLK_W)) m_Camera->MoveForward(dt);
            if (Input::IsKeyDown(SDLK_S)) m_Camera->MoveBackward(dt);
            if (Input::IsKeyDown(SDLK_A)) m_Camera->MoveLeft(dt);
            if (Input::IsKeyDown(SDLK_D)) m_Camera->MoveRight(dt);
            if (Input::IsKeyDown(SDLK_SPACE)) m_Camera->MoveUp(dt);
            if (Input::IsKeyDown(SDLK_LSHIFT)) m_Camera->MoveDown(dt);
        } else {
            /**
             * @brief Handle movement for the local player entity.
             */
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

                        m_Camera->m_Position = tf.position + glm::vec3(0, 2, 0); // Eye height offset

                        tf.rotation.y = m_Camera->m_Yaw;
                        tf.rotation.x = m_Camera->m_Pitch;
                        break;
                    }
                }
            }
        }
    }
    m_Camera->Update(dt);

    m_TotalTime += dt;
    m_FrameCount++;

    /**
     * @brief Render ImGui overlay for gameplay information.
     */
    ImGui::Begin("Game");
    ImGui::Text("Mode: %s", ctx.network.IsHosting() ? "Host" : "Client");
    ImGui::Text("Control: %s (F1)", m_FreeFly ? "Free Fly" : "Player");
    ImGui::Text("Mouse: %s (F2)", Input::IsRelativeMouseMode() ? "Captured" : "Visible");
    if (m_IdAssigned)
        ImGui::Text("playerId=%u  netId=%u", m_MyPlayerId, m_MyNetId);
    else
        ImGui::Text("Waiting for server assignment...");
    if (ImGui::Button("Disconnect")) {
        ctx.network.Disconnect();
        ctx.scenes.RequestTransition(new MainMenuScene());
    }
    ImGui::End();

    // ----------------------------------------------------------------
    // Shadow Debug — live-tune bias and frustum size without recompiling
    // ----------------------------------------------------------------
    ImGui::Begin("Shadow Debug");
    ImGui::TextDisabled("GPU Depth Bias (rebuilds pipeline on change)");
    ImGui::SliderFloat("Bias Constant", &m_ShadowBiasConstant, 0.0f, 10.0f);
    ImGui::SliderFloat("Bias Slope",    &m_ShadowBiasSlope,    0.0f, 10.0f);
    ImGui::Spacing();
    ImGui::TextDisabled("Shadow Frustum");
    ImGui::SliderFloat("Ortho Size",    &m_ShadowOrthoSize,    5.0f, 50.0f);
    ImGui::Spacing();
    ImGui::TextDisabled("Acne = raise Bias | Peter-Pan = lower Bias");
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Fixed-rate update
// ---------------------------------------------------------------------------

/**
 * @brief Handles logic and networking at a fixed rate.
 * @param ctx The scene context.
 * @param dt Fixed delta time.
 */
void GameScene::FixedUpdate(SceneContext& ctx, float dt) {
    if (ctx.network.IsHosting()) {
        PollConnectionEvents(ctx);
        PollClientPackets(ctx);
    }

    if (ctx.network.IsConnected()) {
        PollServerPackets(ctx);
    }

    if (ctx.network.IsHosting()) {
        Systems::MovementSystem(ctx.serverRegistry, dt);
    }

    SendLocalInput(ctx);

    if (ctx.network.IsHosting()) {
        m_SnapAccum += dt;
        if (m_SnapAccum >= SNAPSHOT_RATE) {
            SendSnapshots(ctx);
            m_SnapAccum -= SNAPSHOT_RATE;
        }
    }
}

// ---------------------------------------------------------------------------
// Server-side: connection / disconnection
// ---------------------------------------------------------------------------

/**
 * @brief Polls for network connection events and manages networked entities accordingly.
 * @param ctx The scene context.
 */
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
                    AssetJoinedPacket pkt;
                    pkt.netId = netId;
                    std::strncpy(pkt.modelPath, m->modelPath.c_str(), sizeof(pkt.modelPath)-1);
                    pkt.x = t.position.x; pkt.y = t.position.y; pkt.z = t.position.z;
                    ctx.network.SendToClient(peerId, pkt);
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
            /**
             * @brief Handle player disconnection.
             */
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

// ---------------------------------------------------------------------------
// Server-side: process client input
// ---------------------------------------------------------------------------

/**
 * @brief Processes movement packets received from clients.
 * @param ctx The scene context.
 */
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

// ---------------------------------------------------------------------------
// Server-side: broadcast snapshots
// ---------------------------------------------------------------------------

/**
 * @brief Sends authoritative entity snapshots to all connected clients.
 * @param ctx The scene context.
 */
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

// ---------------------------------------------------------------------------
// Client-side: process server packets
// ---------------------------------------------------------------------------

/**
 * @brief Polls and processes all incoming packets from the server.
 * @param ctx The scene context.
 */
void GameScene::PollServerPackets(SceneContext& ctx) {
    // Identity assignment
    if (!m_IdAssigned) {
        auto idPkt = ctx.network.ReceiveFromServer<PlayerIdAssignPacket>(PacketType::PLAYER_ID_ASSIGN);
        if (idPkt) {
            m_MyPlayerId = idPkt->playerId;
            m_MyNetId    = idPkt->netId;
            m_IdAssigned = true;
            spdlog::info("GameScene: assigned playerId={} netId={}", m_MyPlayerId, m_MyNetId);
        }
    }

    // New entity joins
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<PlayerJoinedPacket>(PacketType::PLAYER_JOINED);
        if (!pkt) break;
        if (m_ClientNetMap.count(pkt->netId)) continue;

        auto entity = ctx.clientRegistry.create();
        ctx.clientRegistry.emplace<TransformComponent>(entity, glm::vec3{pkt->x, pkt->y, pkt->z});
        ctx.clientRegistry.emplace<MovementComponent>(entity);
        ctx.clientRegistry.emplace<PlayerComponent>(entity, pkt->playerId,
                                                     pkt->playerId == m_MyPlayerId);
        ctx.clientRegistry.emplace<NetworkedComponent>(entity, pkt->netId);
        m_ClientNetMap[pkt->netId] = entity;

        spdlog::info("GameScene: client entity created  netId={} playerId={}",
                     pkt->netId, pkt->playerId);
    }

    // Removed entities
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<PlayerLeftPacket>(PacketType::PLAYER_LEFT);
        if (!pkt) break;
        auto it = m_ClientNetMap.find(pkt->netId);
        if (it != m_ClientNetMap.end()) {
            ctx.clientRegistry.destroy(it->second);
            m_ClientNetMap.erase(it);
        }
    }

    // Entity state snapshots
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

            // Simple smoothing (Lerp)
            t->position = glm::lerp(t->position, glm::vec3{pkt->x, pkt->y, pkt->z}, 0.5f);

            if (!isLocalPlayer) {
                t->rotation = glm::lerp(t->rotation, glm::vec3{pkt->rx, pkt->ry, pkt->rz}, 0.5f);
            }

            t->scale = glm::lerp(t->scale, glm::vec3{pkt->sx, pkt->sy, pkt->sz}, 0.5f);
        }
        if (m) m->velocity = {pkt->vx, pkt->vy, pkt->vz};
    }

    // Asset joins
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<AssetJoinedPacket>(PacketType::ASSET_JOINED);
        if (!pkt) break;
        if (m_ClientNetMap.count(pkt->netId)) continue;

        auto entity = ctx.clientRegistry.create();
        ctx.clientRegistry.emplace<TransformComponent>(entity, glm::vec3{pkt->x, pkt->y, pkt->z});
        ctx.clientRegistry.emplace<NetworkedComponent>(entity, pkt->netId);
        ctx.clientRegistry.emplace<ModelComponent>(entity, std::string(pkt->modelPath));
        m_ClientNetMap[pkt->netId] = entity;

        spdlog::info("GameScene: asset entity created  netId={} path={}",
                     pkt->netId, pkt->modelPath);
    }
}

// ---------------------------------------------------------------------------
// Client-side: send local input
// ---------------------------------------------------------------------------

/**
 * @brief Sends the local player's input commands to the server.
 * @param ctx The scene context.
 */
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
    pkt.dx = moveDir.x;
    pkt.dy = moveDir.y;
    pkt.dz = moveDir.z;
    pkt.yaw = m_Camera->m_Yaw;
    pkt.pitch = m_Camera->m_Pitch;
    ctx.network.Send(pkt);
}
