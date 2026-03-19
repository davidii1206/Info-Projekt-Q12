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
    m_Scene = AssetManager::LoadScene("assets/SponzaModel.glb");
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
    config.cullMode = SDL_GPU_CULLMODE_NONE; // Disable culling for testing

    SDL_GPUTextureFormat swapchainFormat = SDL_GetGPUSwapchainTextureFormat(renderer->GetDevice(), renderer->GetWindow()->handle);
    m_Pipeline = renderer->GetPipelines()->CreatePipeline("ModelPipeline", config, swapchainFormat);
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
    
    // screen: xy: resolution, zw: padding
    m_Globals.screen = glm::vec4((float)w, (float)h, 0.0f, 0.0f);

    renderer->UpdateGlobalUniforms(m_Globals);

    // Add Render Pass
    renderer->AddPass("MainPass", nullptr, [this, renderer](RenderContext& ctx) {
        if (!m_Pipeline || !m_Scene.model) return;

        ctx.BindPipeline(m_Pipeline);
        
        // Bind Global Uniforms as a Storage Buffer (Slot 0 in both stages)
        ctx.BindVertexStorageBuffer(0, renderer->GetGlobalUBO());
        ctx.BindFragmentStorageBuffer(0, renderer->GetGlobalUBO());

        ctx.BindVertexBuffer(m_Scene.model->GetVertexBuffer());
        ctx.BindIndexBuffer(m_Scene.model->GetIndexBuffer());

        // Bind Material SSBO to Slot 1 (Set 2 in Fragment, Binding 1)
        if (m_Scene.model->GetMaterialBuffer()) {
            ctx.BindFragmentStorageBuffer(1, m_Scene.model->GetMaterialBuffer());
        }

        const auto& allSections = m_Scene.model->GetSections();
        const auto& allMaterials = m_Scene.model->GetMaterials();

        // Loop over instances from GLTF
        for (const auto& instance : m_Scene.meshInstances) {
            struct ModelPC {
                glm::mat4 model;
            } modelPC;
            modelPC.model = instance.transform;

            // Push Model Matrix to Slot 0
            ctx.PushVertexConstants(0, &modelPC, sizeof(ModelPC));

            // Render all sections belonging to this mesh
            for (uint32_t i = 0; i < instance.sectionCount; ++i) {
                const auto& section = allSections[instance.firstSection + i];

                // Push Material Index to Slot 0 (as vec4 for alignment)
                glm::vec4 matIdx = glm::vec4((float)section.materialIndex, 0.0f, 0.0f, 0.0f);
                ctx.PushFragmentConstants(0, &matIdx, sizeof(glm::vec4));

                // Bind Texture to Slot 0 (Set 0 in Fragment, Binding 0)
                if (section.materialIndex < allMaterials.size()) {
                    auto tex = allMaterials[section.materialIndex].baseColorTexture;
                    if (!tex) tex = AssetManager::GetFallbackTexture();
                    ctx.BindFragmentTexture(0, tex.get());
                }

                ctx.DrawIndexed(section.indexCount, 1, section.firstIndex);
            }
        }
    });
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

    ImGui::End();
}
