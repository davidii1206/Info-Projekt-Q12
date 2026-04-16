/**
 * @file Renderer.h
 * @brief Core rendering system for the Bugmin engine.
 */
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
 * @class Renderer
 * @brief Core rendering class managing the SDL3 GPU device and frame lifecycle.
 * 
 * This class handles initialization of the GPU device, swapchain management,
 * and coordinates the frame graph and pipeline library.
 */
class Renderer {
public:
    /**
     * @brief Constructs a new Renderer.
     * @param window Pointer to the system window.
     */
    Renderer(Window* window);

    /**
     * @brief Destroys the Renderer and releases GPU resources.
     */
    ~Renderer();

    /**
     * @brief Prepares for a new frame by acquiring a command buffer and swapchain texture.
     * @return True if acquisition succeeded, false otherwise.
     */
    bool BeginFrame();

    /**
     * @brief Updates the global uniforms for the current frame.
     * @param uniforms The updated GlobalUniforms structure.
     */
    void UpdateGlobalUniforms(const GlobalUniforms& uniforms);
    
    /**
     * @brief Executes the frame graph, renders UI, and submits the command buffer.
     */
    void EndFrame();

    /**
     * @brief Adds a pass to the current frame's execution graph.
     * @param name Name of the pass for debugging.
     * @param target Pointer to the target Framebuffer.
     * @param func Function to execute for the pass.
     * @param needsDepth Whether the pass needs a depth buffer.
     * @param preFunc Optional function to execute before the pass (e.g., for clearing).
     */
    void AddPass(const std::string& name, Framebuffer* target, std::function<void(RenderContext&)> func, bool needsDepth = true, std::function<void(SDL_GPUCommandBuffer*)> preFunc = nullptr);

    /**
     * @brief Sets the current G-Buffer.
     * @param gbuffer Pointer to the G-Buffer framebuffer.
     */
    void SetGBuffer(Framebuffer* gbuffer) { m_CurrentGBuffer = gbuffer; }

    /**
     * @brief Gets the current G-Buffer.
     * @return Pointer to the G-Buffer framebuffer.
     */
    Framebuffer* GetGBuffer() const { return m_CurrentGBuffer; }

    /**
     * @brief Gets the global uniforms.
     * @return Reference to GlobalUniforms.
     */
    GlobalUniforms& GetGlobalUniforms() { return m_GlobalUniforms; }

    /**
     * @brief Gets the SDL GPU device.
     * @return Pointer to SDL_GPUDevice.
     */
    SDL_GPUDevice* GetDevice() const { return m_Device; }

    /**
     * @brief Gets the system window.
     * @return Pointer to Window.
     */
    Window* GetWindow() const { return m_Window; }

    /**
     * @brief Gets the pipeline library.
     * @return Pointer to PipelineLibrary.
     */
    PipelineLibrary* GetPipelines() const { return m_PipelineLibrary.get(); }

    /**
     * @brief Gets the global uniform buffer object.
     * @return Pointer to GPUBuffer.
     */
    GPUBuffer* GetGlobalUBO() const { return m_GlobalUBO.get(); }

private:
    Window* m_Window; /**< Pointer to the system window. */
    SDL_GPUDevice* m_Device; /**< The SDL GPU device handle. */
    SDL_GPUCommandBuffer* m_CurrentCommandBuffer; /**< Command buffer for the current frame. */
    SDL_GPUTexture* m_CurrentSwapchainTexture; /**< Swapchain texture for the current frame. */

    std::unique_ptr<FrameGraph> m_FrameGraph; /**< The frame graph for managing render passes. */
    std::unique_ptr<PipelineLibrary> m_PipelineLibrary; /**< Library for graphics and compute pipelines. */
    std::unique_ptr<GPUBuffer> m_GlobalUBO; /**< GPU buffer for global uniforms. */
    GlobalUniforms m_GlobalUniforms{}; /**< Local copy of global uniforms. */
    Framebuffer* m_CurrentGBuffer = nullptr; /**< Pointer to the current G-Buffer. */
};