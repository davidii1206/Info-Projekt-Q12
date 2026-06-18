/**
 * @file Renderer.cpp
 * @brief Implementation of the core rendering system.
 */
#include "Renderer.h"
#include <spdlog/spdlog.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

Renderer::Renderer(Window* window) 
    : m_Window(window), m_Device(nullptr), m_CurrentCommandBuffer(nullptr), 
      m_CurrentSwapchainTexture(nullptr) 
{
    // Let SDL select the best available GPU backend (DX12 on Windows, Metal on macOS, Vulkan on Linux)
    m_Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXBC | SDL_GPU_SHADERFORMAT_MSL, false, nullptr);

    if (!m_Device) {
        spdlog::critical("Failed to create SDL GPU Device: {}", SDL_GetError());
        return;
    }

    if (!SDL_ClaimWindowForGPUDevice(m_Device, m_Window->handle)) {
        spdlog::critical("Failed to claim window for SDL GPU Device: {}", SDL_GetError());
        return;
    }

    spdlog::info("SDL3 GPU Renderer Initialized! Backend: {}", SDL_GetGPUDeviceDriver(m_Device));

    // Initialize core rendering sub-systems
    m_FrameGraph = std::make_unique<FrameGraph>(m_Device);
    m_PipelineLibrary = std::make_unique<PipelineLibrary>(m_Device);
    m_GlobalUBO = std::make_unique<GPUBuffer>(m_Device, BufferUsage::Uniform, sizeof(GlobalUniforms) + 256); // Extra space for alignment safety

    // Initialize ImGui for SDL3 and SDL_GPU
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForSDLGPU(m_Window->handle);
    
    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device = m_Device;
    init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(m_Device, m_Window->handle);
    ImGui_ImplSDLGPU3_Init(&init_info);
}

Renderer::~Renderer() {
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    m_GlobalUBO.reset();
    m_PipelineLibrary.reset();
    m_FrameGraph.reset();

    if (m_Device) {
        SDL_ReleaseWindowFromGPUDevice(m_Device, m_Window->handle);
        SDL_DestroyGPUDevice(m_Device);
    }
}

void Renderer::FlushAndWait()
{
    // 1. Merken, ob wir uns mitten in einem aktiven Frame befanden
    bool wasInFrame = (m_CurrentCommandBuffer != nullptr);

    if (m_CurrentCommandBuffer) {
        // 2. Falls ImGui aktiv ist, den Frame ordnungsgemäß schließen
        if (m_ImGuiFrameActive) {
            ImGui::EndFrame();
            m_ImGuiFrameActive = false;
        }

        m_FrameGraph->Reset();

        SDL_SubmitGPUCommandBuffer(m_CurrentCommandBuffer);

        m_CurrentCommandBuffer = nullptr;
        m_CurrentSwapchainTexture = nullptr;
    }

    // Warten, bis die GPU bereit ist (wichtig für das sichere Löschen des alten G-Buffers)
    SDL_WaitForGPUIdle(m_Device);

    // 3. Wenn wir mitten in einem Frame waren, neuen CommandBuffer und Textur holen
    if (wasInFrame) {
        m_CurrentCommandBuffer = SDL_AcquireGPUCommandBuffer(m_Device);
        if (m_CurrentCommandBuffer) {
            // HIER DIE ABSICHERUNG EINBAUEN:
            if (!SDL_WaitAndAcquireGPUSwapchainTexture(
                m_CurrentCommandBuffer,
                m_Window->handle,
                &m_CurrentSwapchainTexture,
                nullptr,
                nullptr
            )) {
                // Falls das Anfordern fehlschlägt (z.B. beim schnellen Resize im Fullscreen):
                m_CurrentSwapchainTexture = nullptr;
                SDL_SubmitGPUCommandBuffer(m_CurrentCommandBuffer);
                m_CurrentCommandBuffer = nullptr; // Wichtig! Verhindert, dass EndFrame() rendern will
            }
        }
    }
}

void Renderer::RestartImGuiFrame() {
    // Only start a new ImGui frame if one isn't already active.
    // This guards against accidental double-calls (e.g. BeginFrame() followed by
    // RestartImGuiFrame() when no FlushAndWait() was needed in between).
    if (!m_ImGuiFrameActive) {
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        m_ImGuiFrameActive = true;
    }
}

bool Renderer::BeginFrame() {
    m_CurrentCommandBuffer = SDL_AcquireGPUCommandBuffer(m_Device);
    if (!m_CurrentCommandBuffer) return false;

    // Acquire swapchain texture for rendering
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(m_CurrentCommandBuffer, m_Window->handle, &m_CurrentSwapchainTexture, nullptr, nullptr)) {
        m_CurrentSwapchainTexture = nullptr;
        SDL_SubmitGPUCommandBuffer(m_CurrentCommandBuffer);
        m_CurrentCommandBuffer = nullptr;
        return false;
    }

    assert(!m_ImGuiFrameActive);
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    m_ImGuiFrameActive = true;
    return true;
}

void Renderer::UpdateGlobalUniforms(const GlobalUniforms& uniforms) {
    m_GlobalUniforms = uniforms;
    if (m_CurrentCommandBuffer) {
        m_GlobalUBO->Upload(&m_GlobalUniforms, sizeof(GlobalUniforms), 0, m_CurrentCommandBuffer, true);
    }
}

void Renderer::AddPass(const std::string& name, Framebuffer* target, std::function<void(RenderContext&)> func, bool needsDepth, std::function<void(SDL_GPUCommandBuffer*)> preFunc, float depthClearValue) {
    m_FrameGraph->AddPass(name, target, func, needsDepth, preFunc, depthClearValue);
}

void Renderer::EndFrame() {


    if (!m_CurrentCommandBuffer) {
        m_ImGuiFrameActive = false;
        ImGui::EndFrame();


        return;
    }



    int w, h;
    SDL_GetWindowSizeInPixels(m_Window->handle, &w, &h);



    // Execute the recorded frame graph
    m_FrameGraph->Execute(
        m_CurrentCommandBuffer,
        m_CurrentSwapchainTexture,
        (uint32_t)w,
        (uint32_t)h
    );



    m_FrameGraph->Reset();



    // Render ImGui overlay
    ImGui::Render();



    m_ImGuiFrameActive = false;



    ImDrawData* drawData = ImGui::GetDrawData();

    if (drawData && m_CurrentCommandBuffer) {
        ImGui_ImplSDLGPU3_PrepareDrawData(drawData, m_CurrentCommandBuffer);

        if (m_CurrentSwapchainTexture) {
            SDL_GPUColorTargetInfo colorTarget = {};
            colorTarget.texture = m_CurrentSwapchainTexture;
            colorTarget.load_op = SDL_GPU_LOADOP_LOAD;
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPURenderPass* uiPass =
                SDL_BeginGPURenderPass(
                    m_CurrentCommandBuffer,
                    &colorTarget,
                    1,
                    nullptr
                );

            if (uiPass) {
                ImGui_ImplSDLGPU3_RenderDrawData(
                    drawData,
                    m_CurrentCommandBuffer,
                    uiPass
                );

                SDL_EndGPURenderPass(uiPass);
            }
        }
    }



    SDL_SubmitGPUCommandBuffer(m_CurrentCommandBuffer);



    m_CurrentCommandBuffer = nullptr;
    m_CurrentSwapchainTexture = nullptr;


}