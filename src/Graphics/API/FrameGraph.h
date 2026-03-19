#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "RenderContext.h"
#include "Framebuffer.h"
#include "Texture.h"

enum class PassType {
    Graphics,
    Compute
};

/**
 * @struct RenderPassDesc
 * @brief Defines a single node in the FrameGraph.
 */
struct RenderPassDesc {
    std::string name;
    PassType type;
    
    // Outputs (Graphics Only)
    Framebuffer* target = nullptr; 
    bool needsDepth = true; // For swapchain passes mostly
    
    // Commands to run BEFORE the pass begins (e.g. PushConstants)
    std::function<void(SDL_GPUCommandBuffer*)> preExecute;
    
    // The actual draw/compute commands
    std::function<void(RenderContext&)> execute;
};

/**
 * @class FrameGraph
 * @brief High-level manager for rendering and compute passes.
 */
#include <unordered_set>

class FrameGraph {
public:
    FrameGraph(SDL_GPUDevice* device);
    ~FrameGraph();

    /** @brief Adds a graphics pass. */
    void AddPass(const std::string& name, Framebuffer* target, std::function<void(RenderContext&)> func, bool needsDepth = true, std::function<void(SDL_GPUCommandBuffer*)> preFunc = nullptr);

    /** @brief Adds a compute pass. */
    void AddComputePass(const std::string& name, std::function<void(RenderContext&)> func, std::function<void(SDL_GPUCommandBuffer*)> preFunc = nullptr);

    /** @brief Executes all recorded passes. */
    void Execute(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* swapchainTexture, uint32_t width, uint32_t height);

    /** @brief Clears all passes for the next frame. */
    void Reset();

private:
    SDL_GPUDevice* m_Device;
    std::vector<RenderPassDesc> m_Passes;
    std::unique_ptr<Texture> m_SwapchainDepth;
    
    std::unordered_set<void*> m_ClearedTargets;
};
