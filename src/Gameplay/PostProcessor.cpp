/**
 * @file PostProcessor.cpp
 * @brief Implementation of the post-processing pipeline and G-Buffer management.
 */

#include "PostProcessor.h"
#include "Graphics/Renderer.h"
#include "Graphics/API/PipelineLibrary.h"
#include "Graphics/API/Shader.h"
#include <imgui.h>
#include <spdlog/spdlog.h>

/**
 * @brief Constructs the post-processor and defines G-Buffer formats.
 * @param renderer Pointer to the renderer instance.
 */
PostProcessor::PostProcessor(Renderer* renderer) {
    /**
     * @brief Define formats for each G-Buffer target.
     * 0: Normal, 1: Color (Albedo), 2: Light Buffer, 3: Object ID
     */
    m_GBufferFormats = {
        SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, // Normal
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,     // Color (Albedo)
        SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, // Light Buffer
        SDL_GPU_TEXTUREFORMAT_R16_UINT            // Object ID
    };

    InitPipelines(renderer);
    spdlog::info("PostProcessor Initialized");
}

PostProcessor::~PostProcessor() {}

/**
 * @brief Initializes shaders and graphics pipelines for post-processing.
 * @param renderer Pointer to the renderer instance.
 */
void PostProcessor::InitPipelines(Renderer* renderer) {
    SDL_GPUTextureFormat swapchainFormat = SDL_GetGPUSwapchainTextureFormat(renderer->GetDevice(), renderer->GetWindow()->handle);

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

void PostProcessor::Update(float deltaTime) {}

/**
 * @brief Prepares for the current frame by resizing the G-Buffer and updating uniforms.
 * @param renderer Pointer to the renderer instance.
 */
void PostProcessor::BeginFrame(Renderer* renderer) {
    int w, h;
    SDL_GetWindowSizeInPixels(renderer->GetWindow()->handle, &w, &h);

    /**
     * @brief Handle G-Buffer lifecycle and resizing.
     * Resizes G-Buffer if window dimensions change or downscale factor changes.
     */
    uint32_t lowW = (uint32_t)w / m_DownscaleFactor;
    uint32_t lowH = (uint32_t)h / m_DownscaleFactor;
    if (lowW == 0) lowW = 1;
    if (lowH == 0) lowH = 1;

    if (!m_GBuffer || m_GBuffer->GetWidth() != lowW || m_GBuffer->GetHeight() != lowH) {
        renderer->FlushAndWait();

        m_GBuffer = std::make_unique<Framebuffer>(
            renderer->GetDevice(),
            lowW,
            lowH,
            m_GBufferFormats,
            true
        );
        renderer->RestartImGuiFrame();

        spdlog::info("PostProcessor: Resized G-Buffer to {}x{}", lowW, lowH);
    }
    renderer->SetGBuffer(m_GBuffer.get());

    /**
     * @brief Clear the G-Buffer every frame.
     */
    renderer->AddPass("ClearGBuffer", m_GBuffer.get(), [](RenderContext& ctx) {}, true);

    /**
     * @brief Update screen-related global uniforms for the geometry pass.
     */
    GlobalUniforms& globals = renderer->GetGlobalUniforms();
    globals.screen = glm::vec4((float)lowW, (float)lowH, m_PosterizeSteps, 0.0f);
    renderer->UpdateGlobalUniforms(globals);
}

/**
 * @brief Records the final post-processing pass into the renderer.
 * @param renderer Pointer to the renderer instance.
 */
void PostProcessor::EndFrame(Renderer* renderer) {
    int w, h;
    SDL_GetWindowSizeInPixels(renderer->GetWindow()->handle, &w, &h);
    uint32_t lowW = m_GBuffer->GetWidth();
    uint32_t lowH = m_GBuffer->GetHeight();

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

/**
 * @brief Renders the ImGui debug UI for controlling post-processing parameters.
 */
void PostProcessor::OnImGui() {
    ImGui::Begin("Post-Processing Settings");

    // 1. Temporäre statische Variable für das flüssige Ziehen im UI
    static int visualScale = m_DownscaleFactor;

    // 2. Der Slider modifiziert nur den visuellen Wert (kein FPS-Drop beim Ziehen!)
    ImGui::SliderInt("Pixel Size", &visualScale, 1, 8);

    // 3. ERST WENN DER USER LOSLÄSST: Den echten Wert übernehmen und das Resize triggern
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        m_DownscaleFactor = visualScale;
    }
    // 4. Synchronisation: Wenn der Slider nicht aktiv ist, halten wir ihn synchron mit dem echten Wert
    else if (!ImGui::IsItemActive()) {
        visualScale = m_DownscaleFactor;
    }

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