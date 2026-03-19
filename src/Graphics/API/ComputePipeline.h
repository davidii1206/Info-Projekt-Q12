#pragma once
#include <SDL3/SDL_gpu.h>
#include <string>

/**
 * @struct ComputeConfig
 * @brief Configuration for a Compute Pipeline.
 */
struct ComputeConfig {
    size_t codeSize;
    const uint8_t* code;
    const char* entrypoint = "main";
    SDL_GPUShaderFormat format;

    uint32_t numSamplers = 0;
    uint32_t numReadonlyStorageTextures = 0;
    uint32_t numReadonlyStorageBuffers = 0;
    uint32_t numReadwriteStorageTextures = 0;
    uint32_t numReadwriteStorageBuffers = 0;
    uint32_t numUniformBuffers = 0;

    uint32_t threadCountX = 1;
    uint32_t threadCountY = 1;
    uint32_t threadCountZ = 1;
};

/**
 * @class ComputePipeline
 * @brief Wrapper for SDL_GPUComputePipeline.
 * 
 * Used for general purpose GPU computing tasks (physics, particles, etc.)
 */
class ComputePipeline {
public:
    ComputePipeline(SDL_GPUDevice* device, const ComputeConfig& config);
    ~ComputePipeline();

    SDL_GPUComputePipeline* GetHandle() const { return m_Pipeline; }

private:
    SDL_GPUDevice* m_Device;
    SDL_GPUComputePipeline* m_Pipeline;
};
