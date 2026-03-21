#include "TestScene.h"
#include "Core/AssetManager.h"
#include "Core/Input.h"
#include "Graphics/API/PipelineLibrary.h"
#include "Graphics/API/Shader.h"
#include <imgui.h>
#include <spdlog/spdlog.h>

TestScene::TestScene(Renderer* renderer) {
    m_WindowHandle = renderer->GetWindow()->handle;
    m_Camera = std::make_unique<Camera>();
    m_Camera->m_Position = {0, 0, 0};

    // Attempt to load a scene
    m_Scene = AssetManager::LoadScene("assets/test_scene_pixelation.glb");
    if (!m_Scene.model) {
        spdlog::warn("TestScene: Failed to load SponzaModel.glb, using fallback.");
        m_Scene.model = AssetManager::GetFallbackModel();
        MeshInstance fallbackInstance;
        fallbackInstance.firstSection = 0;
        fallbackInstance.sectionCount = (uint32_t)m_Scene.model->GetSections().size();
        fallbackInstance.transform = glm::mat4(1.0f);
        m_Scene.meshInstances.push_back(fallbackInstance);
    }

    InitPipelines(renderer);
    spdlog::info("TestScene Initialized");
}

TestScene::~TestScene() {
    if (m_WindowHandle && Input::IsRelativeMouseMode()) {
        Input::SetRelativeMouseMode(m_WindowHandle, false);
    }
}


void TestScene::InitPipelines(Renderer* renderer) {
    // Vertex: 1 Storage Buffer (Slot 0: globals), 1 Uniform Buffer (Slot 0: pc)
    ShaderResourceLayout vertLayout = {0, 0, 1, 1}; 
    // Fragment: 1 Sampler (Slot 0), 2 Storage Buffers (Slot 0: Globals, Slot 1: Material), 1 Uniform Buffer (Slot 0: pc)
    ShaderResourceLayout fragLayout = {1, 0, 2, 1}; 

    // Shaders are in the 'shaders/' directory relative to the executable
    m_VertShader = std::make_unique<Shader>(renderer->GetDevice(), "shaders/model.vert.spv", ShaderStage::Vertex, vertLayout);
    m_FragShader = std::make_unique<Shader>(renderer->GetDevice(), "shaders/model.frag.spv", ShaderStage::Fragment, fragLayout);

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
    config.depthCompareOp = SDL_GPU_COMPAREOP_GREATER; // Required for Reverse-Z

    SDL_GPUTextureFormat swapchainFormat = SDL_GetGPUSwapchainTextureFormat(renderer->GetDevice(), renderer->GetWindow()->handle);
    m_Pipeline = renderer->GetPipelines()->CreatePipeline("ModelPipeline", config, swapchainFormat);

    // --- G-Buffer Setup ---
    m_GBufferFormats = {
        SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, // Normal
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,     // Color (Albedo)
        SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, // Light Buffer
        SDL_GPU_TEXTUREFORMAT_R16_UINT            // Object ID
    };

    PipelineConfig gBufferConfig = config; // Reuse geometry state
    gBufferConfig.colorTargetFormats = m_GBufferFormats;
    
    m_GBufferPipeline = renderer->GetPipelines()->CreatePipeline("GBufferPipeline", gBufferConfig, SDL_GPU_TEXTUREFORMAT_INVALID);

    // --- Post-Processing Setup ---
    ShaderResourceLayout postVertLayout = {0, 0, 0, 0}; // Fullscreen triangle
    ShaderResourceLayout postFragLayout = {4, 0, 0, 2}; // 4 Samplers, 2 PC slots

    m_PostVertShader = std::make_unique<Shader>(renderer->GetDevice(), "shaders/post.vert.spv", ShaderStage::Vertex, postVertLayout);
    m_PostFragShader = std::make_unique<Shader>(renderer->GetDevice(), "shaders/post.frag.spv", ShaderStage::Fragment, postFragLayout);

    PipelineConfig postConfig;
    postConfig.vertexShader = m_PostVertShader.get();
    postConfig.fragmentShader = m_PostFragShader.get();
    postConfig.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    postConfig.enableDepthTest = false;
    postConfig.enableBlending = false;

    m_PostPipeline = renderer->GetPipelines()->CreatePipeline("PostPipeline", postConfig, swapchainFormat);
}

void TestScene::Update(float deltaTime) {
    if (Input::IsKeyPressed(SDLK_F1)) {
        bool newState = !Input::IsRelativeMouseMode();
        Input::SetRelativeMouseMode(m_WindowHandle, newState);
        spdlog::info("Input Mode: {}", newState ? "3D Camera (Captured)" : "UI Cursor (Free)");
    }

    if (Input::IsRelativeMouseMode()) {
        glm::vec2 delta = Input::GetMouseDelta();
        m_Camera->Rotate(delta.x, delta.y);

        if (Input::IsKeyDown(SDLK_W)) m_Camera->MoveForward(deltaTime);
        if (Input::IsKeyDown(SDLK_S)) m_Camera->MoveBackward(deltaTime);
        if (Input::IsKeyDown(SDLK_A)) m_Camera->MoveLeft(deltaTime);
        if (Input::IsKeyDown(SDLK_D)) m_Camera->MoveRight(deltaTime);
        if (Input::IsKeyDown(SDLK_SPACE)) m_Camera->MoveUp(deltaTime);
        if (Input::IsKeyDown(SDLK_LSHIFT)) m_Camera->MoveDown(deltaTime);
    }

    m_Camera->Update(deltaTime);
    
    m_TotalTime += deltaTime;
    m_FrameCount++;
}

void TestScene::Render(Renderer* renderer) {
    // Update Global Uniforms
    int w, h;
    SDL_GetWindowSizeInPixels(renderer->GetWindow()->handle, &w, &h);
    float aspect = (float)w / (float)h;

    // --- G-Buffer Lifecycle (Low Res) ---
    uint32_t lowW = (uint32_t)w / m_DownscaleFactor;
    uint32_t lowH = (uint32_t)h / m_DownscaleFactor;
    if (lowW == 0) lowW = 1;
    if (lowH == 0) lowH = 1;

    if (!m_GBuffer || m_GBuffer->GetWidth() != lowW || m_GBuffer->GetHeight() != lowH) {
        m_GBuffer = std::make_unique<Framebuffer>(renderer->GetDevice(), lowW, lowH, m_GBufferFormats, true);
        spdlog::info("TestScene: Resized G-Buffer to {}x{}", lowW, lowH);
    }

    m_Globals.view = m_Camera->GetViewMatrix();
    m_Globals.proj = m_Camera->GetProjectionMatrix(aspect);
    m_Globals.viewProj = m_Globals.proj * m_Globals.view;
    m_Globals.cameraPos = glm::vec4(m_Camera->m_Position, 1.0f);
    
    // Fill light data from the scene
    uint32_t lightCount = std::min((uint32_t)m_Scene.lights.size(), 16u);
    for (uint32_t i = 0; i < lightCount; ++i) {
        m_Globals.lights[i] = m_Scene.lights[i];
    }

    // timers: x: time, y: numLights, z: deltaTime, w: frameCount
    m_Globals.timers = glm::vec4(m_TotalTime, (float)lightCount, 0.016f, (float)m_FrameCount);
    
    // screen: xy: resolution, z: posterizeSteps, w: padding
    m_Globals.screen = glm::vec4((float)lowW, (float)lowH, m_PosterizeSteps, 0.0f);

    renderer->UpdateGlobalUniforms(m_Globals);

    // --- 1. G-Buffer Pass (Low Res) ---
    renderer->AddPass("GBufferPass", m_GBuffer.get(), [this, renderer](RenderContext& ctx) {
        if (!m_GBufferPipeline || !m_Scene.model) return;

        ctx.BindPipeline(m_GBufferPipeline);
        
        ctx.BindVertexStorageBuffer(0, renderer->GetGlobalUBO());
        ctx.BindFragmentStorageBuffer(0, renderer->GetGlobalUBO());

        ctx.BindVertexBuffer(m_Scene.model->GetVertexBuffer());
        ctx.BindIndexBuffer(m_Scene.model->GetIndexBuffer());

        if (m_Scene.model->GetMaterialBuffer()) {
            ctx.BindFragmentStorageBuffer(1, m_Scene.model->GetMaterialBuffer());
        }

        const auto& allSections = m_Scene.model->GetSections();
        const auto& allMaterials = m_Scene.model->GetMaterials();

        uint32_t instanceID = 1; // Start from 1, 0 is background
        for (const auto& instance : m_Scene.meshInstances) {
            struct ModelPC { glm::mat4 model; } modelPC;
            modelPC.model = instance.transform;
            ctx.PushVertexConstants(0, &modelPC, sizeof(ModelPC));

            for (uint32_t i = 0; i < instance.sectionCount; ++i) {
                const auto& section = allSections[instance.firstSection + i];
                
                struct FragPC {
                    uint32_t matIdx;
                    uint32_t objID;
                    uint32_t pad1;
                    uint32_t pad2;
                } fpc;
                fpc.matIdx = (uint32_t)section.materialIndex;
                fpc.objID = instanceID;
                ctx.PushFragmentConstants(0, &fpc, sizeof(FragPC));

                if (section.materialIndex < allMaterials.size()) {
                    auto tex = allMaterials[section.materialIndex].baseColorTexture;
                    if (!tex) tex = AssetManager::GetFallbackTexture();
                    ctx.BindFragmentTexture(0, tex.get());
                }
                ctx.DrawIndexed(section.indexCount, 1, section.firstIndex);
            }
            instanceID++;
        }
    });

    // --- Post-Processing Pass (Swapchain) ---
    renderer->AddPass("PostPass", nullptr, [this, w, h, lowW, lowH](RenderContext& ctx) {
        if (!m_PostPipeline || !m_GBuffer) return;

        ctx.BindPipeline(m_PostPipeline);

        // Bind G-Buffer textures
        ctx.BindFragmentTexture(0, m_GBuffer->GetColorTarget(0)); // tNormal
        ctx.BindFragmentTexture(1, m_GBuffer->GetColorTarget(1)); // tColor
        ctx.BindFragmentTexture(2, m_GBuffer->GetColorTarget(2)); // tLight
        ctx.BindFragmentTexture(3, m_GBuffer->GetDepthTarget());   // tDepth

        struct PostPC {
            glm::vec4 resolution; // x, y, 1/x, 1/y
            glm::vec4 params;     // x: normalEdgeStrength, y: depthEdgeStrength, z: posterizeSteps, w: debugMode
        } pc;
        pc.resolution = glm::vec4((float)lowW, (float)lowH, 1.0f / (float)lowW, 1.0f / (float)lowH);
        pc.params = glm::vec4(m_NormalThreshold, m_DepthThreshold, m_PosterizeSteps, (float)m_DebugMode);

        ctx.PushFragmentConstants(0, &pc, sizeof(PostPC));

        ctx.Draw(3); // Fullscreen triangle
    }, false); 
}

void TestScene::OnImGui() {
    ImGui::Begin("Test Scene Debugger");
    
    if (ImGui::Button("Reset Camera")) {
        m_Camera->m_Position = {0, 2, 10};
        m_Camera->m_Yaw = -90.0f;
        m_Camera->m_Pitch = 0.0f;
        m_Camera->UpdateVectors();
    }

    ImGui::Separator();
    ImGui::Text("Camera Controls (F1 to toggle mode)");
    ImGui::DragFloat3("Position", &m_Camera->m_Position.x, 0.1f);
    ImGui::DragFloat("Yaw", &m_Camera->m_Yaw, 0.5f);
    ImGui::DragFloat("Pitch", &m_Camera->m_Pitch, 0.5f);
    ImGui::DragFloat("Speed", &m_Camera->m_MovementSpeed, 0.5f);
    
    if (m_Scene.model) {
        ImGui::Separator();
        ImGui::Text("Model Info");
        ImGui::Text("Sections: %d", (int)m_Scene.model->GetSections().size());
        ImGui::Text("Materials: %d", (int)m_Scene.model->GetMaterials().size());
    }

    ImGui::Separator();
    ImGui::Text("Post-Processing Settings (Three.js Style)");
    ImGui::SliderInt("Pixel Size", &m_DownscaleFactor, 1, 8);
    ImGui::SliderFloat("Normal Edge Strength", &m_NormalThreshold, 0.0f, 1.0f);
    ImGui::SliderFloat("Depth Edge Strength", &m_DepthThreshold, 0.0f, 1.0f);
    ImGui::SliderFloat("Posterize Steps", &m_PosterizeSteps, 1.0f, 16.0f);

    const char* debugModes[] = { "None", "Normal", "Color", "Light", "Depth", "Depth Indicator", "Normal Indicator" };
    ImGui::Combo("Debug Mode", &m_DebugMode, debugModes, IM_ARRAYSIZE(debugModes));

    if (m_GBuffer) {
        ImGui::Separator();
        ImGui::Text("G-Buffer Visualization (Low Res)");
        if (ImGui::BeginTabBar("GBufferTabs")) {
            if (ImGui::BeginTabItem("Normal")) {
                ImTextureID texID = (ImTextureID)m_GBuffer->GetColorTarget(0)->GetHandle();
                ImGui::Image(texID, ImVec2(256, 256));
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Color")) {
                ImTextureID texID = (ImTextureID)m_GBuffer->GetColorTarget(1)->GetHandle();
                ImGui::Image(texID, ImVec2(256, 256));
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Light")) {
                ImTextureID texID = (ImTextureID)m_GBuffer->GetColorTarget(2)->GetHandle();
                ImGui::Image(texID, ImVec2(256, 256));
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Depth")) {
                ImTextureID texID = (ImTextureID)m_GBuffer->GetDepthTarget()->GetHandle();
                ImGui::Image(texID, ImVec2(256, 256));
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    ImGui::End();
}
