/**
 * @file FrameGraph.h
 * @brief High-level management for rendering and compute passes.
 */
#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_set>
#include "RenderContext.h"
#include "Framebuffer.h"
#include "Texture.h"

/**
 * @enum PassType
 * @brief Types of passes supported by the FrameGraph.
 */
enum class PassType {
    Graphics, /**< Graphics rendering pass. */
    Compute   /**< GPGPU compute pass. */
};

/**
 * @struct RenderPassDesc
 * @brief Defines a single node in the FrameGraph.
 */
struct RenderPassDesc {
    std::string name; /**< Name of the pass for debugging. */
    PassType type;    /**< Type of the pass (Graphics or Compute). */
    
    // Outputs (Graphics Only)
    Framebuffer* target = nullptr; /**< Target framebuffer, or nullptr for swapchain. */
    bool needsDepth = true;        /**< Whether the pass needs a depth buffer. */
    float depthClearValue = 0.0f;  /**< Value to clear depth to (0.0 = Far in Reverse-Z, 1.0 = Far in Forward-Z). */
    
    /** @brief Commands to run BEFORE the pass begins (e.g. PushConstants). */
    std::function<void(SDL_GPUCommandBuffer*)> preExecute;
    
    /** @brief The actual draw/compute commands. */
    std::function<void(RenderContext&)> execute;
};

/**
 * @class FrameGraph
 * @brief High-level manager for rendering and compute passes.
 * 
 * The FrameGraph allows for high-level organization of the rendering process,
 * handling pass ordering, target clearing, and automatic viewport management.
 */
class FrameGraph {
public:
    /**
     * @brief Constructs a new FrameGraph.
     * @param device Pointer to the active SDL_GPUDevice.
     */
    FrameGraph(SDL_GPUDevice* device);

    /**
     * @brief Destroys the FrameGraph and releases resources.
     */
    ~FrameGraph();

    /**
     * @brief Adds a graphics pass.
     * @param name Name of the pass.
     * @param target Target framebuffer (nullptr for swapchain).
     * @param func Execution function for the pass.
     * @param needsDepth Whether depth is needed.
     * @param preFunc Optional function to run before the pass.
     * @param depthClearValue Value used to clear the depth attachment (default 0.0).
     */
    void AddPass(const std::string& name, Framebuffer* target, std::function<void(RenderContext&)> func, bool needsDepth = true, std::function<void(SDL_GPUCommandBuffer*)> preFunc = nullptr, float depthClearValue = 0.0f);

    /**
     * @brief Adds a compute pass.
     * @param name Name of the pass.
     * @param func Execution function for the pass.
     * @param preFunc Optional function to run before the pass.
     */
    void AddComputePass(const std::string& name, std::function<void(RenderContext&)> func, std::function<void(SDL_GPUCommandBuffer*)> preFunc = nullptr);

    /**
     * @brief Executes all recorded passes.
     * @param cmd Active command buffer.
     * @param swapchainTexture Current swapchain texture.
     * @param width Current swapchain width.
     * @param height Current swapchain height.
     */
    void Execute(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* swapchainTexture, uint32_t width, uint32_t height);

    /**
     * @brief Clears all passes for the next frame.
     */
    void Reset();

private:
    SDL_GPUDevice* m_Device;                /**< Pointer to the SDL GPU device. */
    std::vector<RenderPassDesc> m_Passes;   /**< List of recorded passes for the current frame. */
    std::unique_ptr<Texture> m_SwapchainDepth; /**< Shared depth buffer for swapchain rendering. */
    
    std::unordered_set<void*> m_ClearedTargets; /**< Track which targets have been cleared this frame. */
};
