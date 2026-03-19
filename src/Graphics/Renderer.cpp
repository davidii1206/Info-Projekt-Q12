#include "Renderer.h"
#include <spdlog/spdlog.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

Renderer::Renderer(Window* window) 
    : m_Window(window), m_Device(nullptr), m_CurrentCommandBuffer(nullptr), 
      m_CurrentSwapchainTexture(nullptr) 
{
    m_Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXBC | SDL_GPU_SHADERFORMAT_MSL, false, "vulkan");
    
    if (!m_Device) {
        spdlog::warn("Vulkan not available or failed to init, falling back to default GPU driver.");
        m_Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXBC | SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
    }

    if (!m_Device) {
        spdlog::critical("Failed to create SDL GPU Device: {}", SDL_GetError());
        return;
    }

    if (!SDL_ClaimWindowForGPUDevice(m_Device, m_Window->handle)) {
        spdlog::critical("Failed to claim window for SDL GPU Device: {}", SDL_GetError());
        return;
    }

    spdlog::info("SDL3 GPU Renderer Initialized! Backend: {}", SDL_GetGPUDeviceDriver(m_Device));

    m_FrameGraph = std::make_unique<FrameGraph>(m_Device);
    m_PipelineLibrary = std::make_unique<PipelineLibrary>(m_Device);
    m_GlobalUBO = std::make_unique<GPUBuffer>(m_Device, BufferUsage::Uniform, sizeof(GlobalUniforms) + 256); // Extra space for alignment safety

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

bool Renderer::BeginFrame() {
    m_CurrentCommandBuffer = SDL_AcquireGPUCommandBuffer(m_Device);
    if (!m_CurrentCommandBuffer) return false;

    if (!SDL_WaitAndAcquireGPUSwapchainTexture(m_CurrentCommandBuffer, m_Window->handle, &m_CurrentSwapchainTexture, nullptr, nullptr)) {
        m_CurrentSwapchainTexture = nullptr;
        SDL_SubmitGPUCommandBuffer(m_CurrentCommandBuffer);
        m_CurrentCommandBuffer = nullptr;
        return false;
    }

    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    return true;
}

void Renderer::UpdateGlobalUniforms(const GlobalUniforms& uniforms) {
    if (m_CurrentCommandBuffer) {
        m_GlobalUBO->Upload(&uniforms, sizeof(GlobalUniforms), 0, m_CurrentCommandBuffer, true);
    }
}

void Renderer::AddPass(const std::string& name, Framebuffer* target, std::function<void(RenderContext&)> func, bool needsDepth, std::function<void(SDL_GPUCommandBuffer*)> preFunc) {
    m_FrameGraph->AddPass(name, target, func, needsDepth, preFunc);
}

void Renderer::EndFrame() {
    if (!m_CurrentCommandBuffer) return;

    int w, h;
    SDL_GetWindowSizeInPixels(m_Window->handle, &w, &h);
    m_FrameGraph->Execute(m_CurrentCommandBuffer, m_CurrentSwapchainTexture, (uint32_t)w, (uint32_t)h);
    m_FrameGraph->Reset();

    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData && m_CurrentCommandBuffer) {
        ImGui_ImplSDLGPU3_PrepareDrawData(drawData, m_CurrentCommandBuffer);

        if (m_CurrentSwapchainTexture) {
            SDL_GPUColorTargetInfo colorTarget = {};
            colorTarget.texture = m_CurrentSwapchainTexture;
            colorTarget.load_op = SDL_GPU_LOADOP_LOAD; 
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPURenderPass* uiPass = SDL_BeginGPURenderPass(m_CurrentCommandBuffer, &colorTarget, 1, nullptr);
            if (uiPass) {
                ImGui_ImplSDLGPU3_RenderDrawData(drawData, m_CurrentCommandBuffer, uiPass);
                SDL_EndGPURenderPass(uiPass);
            }
        }
    }

    SDL_SubmitGPUCommandBuffer(m_CurrentCommandBuffer);
    m_CurrentCommandBuffer = nullptr;
    m_CurrentSwapchainTexture = nullptr;
}
