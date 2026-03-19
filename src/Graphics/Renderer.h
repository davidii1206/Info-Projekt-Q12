#pragma once
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include "Window/Window.h"

#include "API/FrameGraph.h"
#include "API/PipelineLibrary.h"
#include "GlobalUniforms.h"

struct SDL_GPUDevice;
struct SDL_GPUCommandBuffer;
struct SDL_GPURenderPass;
struct SDL_GPUTexture;

/**
 * @brief Core rendering class managing the SDL3 GPU device and frame lifecycle.
 */
class Renderer {
public:
    Renderer(Window* window);
    ~Renderer();

    /**
     * @brief Prepares for a new frame by acquiring a command buffer.
     * @return True if acquisition succeeded, false otherwise.
     */
    bool BeginFrame();

    /**
     * @brief Updates the global uniforms for the current frame.
     */
    void UpdateGlobalUniforms(const GlobalUniforms& uniforms);
    
    /**
     * @brief Executes the frame graph and submits the command buffer.
     */
    void EndFrame();

    /** @brief Adds a pass to the current frame's execution graph. */
    void AddPass(const std::string& name, Framebuffer* target, std::function<void(RenderContext&)> func, bool needsDepth = true, std::function<void(SDL_GPUCommandBuffer*)> preFunc = nullptr);

    SDL_GPUDevice* GetDevice() const { return m_Device; }
    Window* GetWindow() const { return m_Window; }
    PipelineLibrary* GetPipelines() const { return m_PipelineLibrary.get(); }
    GPUBuffer* GetGlobalUBO() const { return m_GlobalUBO.get(); }

private:
    Window* m_Window;
    SDL_GPUDevice* m_Device;
    SDL_GPUCommandBuffer* m_CurrentCommandBuffer;
    SDL_GPUTexture* m_CurrentSwapchainTexture;

    std::unique_ptr<FrameGraph> m_FrameGraph;
    std::unique_ptr<PipelineLibrary> m_PipelineLibrary;
    std::unique_ptr<GPUBuffer> m_GlobalUBO;
};