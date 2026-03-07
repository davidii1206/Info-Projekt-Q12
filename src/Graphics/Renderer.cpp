#include "Renderer.h"
#include <spdlog/spdlog.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

Renderer::Renderer(Window* window) 
    : m_Window(window), m_Device(nullptr), m_CurrentCommandBuffer(nullptr), 
      m_CurrentRenderPass(nullptr), m_CurrentSwapchainTexture(nullptr) 
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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

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

    if (m_Device) {
        SDL_ReleaseWindowFromGPUDevice(m_Device, m_Window->handle);
        SDL_DestroyGPUDevice(m_Device);
    }
}

void Renderer::BeginFrame(const glm::vec4& clearColor) {
    m_CurrentCommandBuffer = SDL_AcquireGPUCommandBuffer(m_Device);
    if (!m_CurrentCommandBuffer) return;

    if (!SDL_WaitAndAcquireGPUSwapchainTexture(m_CurrentCommandBuffer, m_Window->handle, &m_CurrentSwapchainTexture, nullptr, nullptr)) {
        m_CurrentSwapchainTexture = nullptr;
        return;
    }

    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void Renderer::EndFrame() {
    if (!m_CurrentCommandBuffer) return;

    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();

    ImGui_ImplSDLGPU3_PrepareDrawData(drawData, m_CurrentCommandBuffer);

    if (m_CurrentSwapchainTexture) {
        SDL_GPUColorTargetInfo colorTarget = {};
        colorTarget.texture = m_CurrentSwapchainTexture;
        colorTarget.clear_color = { 0.1f, 0.1f, 0.1f, 1.0f }; 
        colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTarget.store_op = SDL_GPU_STOREOP_STORE;

        m_CurrentRenderPass = SDL_BeginGPURenderPass(m_CurrentCommandBuffer, &colorTarget, 1, nullptr);
        
        if (m_CurrentRenderPass) {
            ImGui_ImplSDLGPU3_RenderDrawData(drawData, m_CurrentCommandBuffer, m_CurrentRenderPass);
            SDL_EndGPURenderPass(m_CurrentRenderPass);
            m_CurrentRenderPass = nullptr;
        }
    }

    SDL_SubmitGPUCommandBuffer(m_CurrentCommandBuffer);
    m_CurrentCommandBuffer = nullptr;
    m_CurrentSwapchainTexture = nullptr;
}