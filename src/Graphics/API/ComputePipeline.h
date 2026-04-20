/**
 * @file ComputePipeline.h
 * @brief Compute pipeline management for GPGPU tasks.
 */
#pragma once
#include <SDL3/SDL_gpu.h>
#include <string>

/**
 * @struct ComputeConfig
 * @brief Configuration for a Compute Pipeline.
 */
struct ComputeConfig {
    size_t codeSize;                     /**< Size of the shader binary code. */
    const uint8_t* code;                 /**< Pointer to the shader binary code. */
    const char* entrypoint = "main";     /**< Name of the entry point function. */
    SDL_GPUShaderFormat format;         /**< Binary format of the shader. */

    uint32_t numSamplers = 0;                    /**< Number of samplers bound. */
    uint32_t numReadonlyStorageTextures = 0;     /**< Number of readonly storage textures. */
    uint32_t numReadonlyStorageBuffers = 0;      /**< Number of readonly storage buffers. */
    uint32_t numReadwriteStorageTextures = 0;    /**< Number of read-write storage textures. */
    uint32_t numReadwriteStorageBuffers = 0;     /**< Number of read-write storage buffers. */
    uint32_t numUniformBuffers = 0;              /**< Number of uniform buffers. */

    uint32_t threadCountX = 1; /**< Number of threads in the X dimension. */
    uint32_t threadCountY = 1; /**< Number of threads in the Y dimension. */
    uint32_t threadCountZ = 1; /**< Number of threads in the Z dimension. */
};

/**
 * @class ComputePipeline
 * @brief Wrapper for SDL_GPUComputePipeline.
 * 
 * Used for general purpose GPU computing tasks (physics, particles, etc.)
 */
class ComputePipeline {
public:
    /**
     * @brief Compiles a new compute pipeline.
     * @param device Pointer to the active SDL_GPUDevice.
     * @param config The configuration describing the compute state.
     */
    ComputePipeline(SDL_GPUDevice* device, const ComputeConfig& config);

    /**
     * @brief Destroys the compute pipeline and releases GPU resources.
     */
    ~ComputePipeline();

    /**
     * @brief Gets the native SDL pipeline handle.
     * @return Pointer to SDL_GPUComputePipeline.
     */
    SDL_GPUComputePipeline* GetHandle() const { return m_Pipeline; }

private:
    SDL_GPUDevice* m_Device;            /**< Pointer to the SDL GPU device. */
    SDL_GPUComputePipeline* m_Pipeline;  /**< Native SDL compute pipeline handle. */
};
