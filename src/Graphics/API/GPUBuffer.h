#pragma once
#include <SDL3/SDL_gpu.h>
#include <cstdint>
#include <vector>

/**
 * @enum BufferUsage
 * @brief Defines the intended purpose of a GPU buffer.
 */
enum class BufferUsage {
    Vertex,  ///< Used for vertex data.
    Index,   ///< Used for index data (triangles).
    Uniform, ///< Used for constant/storage data read in shaders.
    Compute  ///< Used for compute shader storage (read/write).
};

/**
 * @class GPUBuffer
 * @brief High-level abstraction for SDL3 GPU Buffers.
 * 
 * Manages the lifecycle of an SDL_GPUBuffer and provides a safe interface 
 * for uploading data from the CPU to GPU memory using internal staging.
 */
class GPUBuffer {
public:
    /**
     * @brief Creates a GPU buffer with a specific usage and size.
     * @param device Pointer to the active SDL_GPUDevice.
     * @param usage The intended usage of this buffer.
     * @param size Size of the buffer in bytes.
     */
    GPUBuffer(SDL_GPUDevice* device, BufferUsage usage, uint32_t size);

    /**
     * @brief Releases the underlying SDL GPU resources.
     */
    ~GPUBuffer();

    /**
     * @brief Uploads data to the GPU buffer.
     * 
     * Internally creates a temporary staging buffer, maps it, copies the 
     * data, and records a transfer command to the GPU.
     * 
     * @param data Pointer to the CPU-side data.
     * @param size Size of data to upload in bytes.
     * @param offset Destination offset within the GPU buffer.
     * @param cmd Optional command buffer to use for the transfer.
     * @param cycle If true, SDL will cycle the buffer to avoid stalls (recommended for per-frame updates).
     */
    void Upload(const void* data, uint32_t size, uint32_t offset = 0, SDL_GPUCommandBuffer* cmd = nullptr, bool cycle = false);

    /** @brief Gets the native SDL handle. */
    SDL_GPUBuffer* GetHandle() const { return m_Buffer; }

    /** @brief Gets the total size of the buffer in bytes. */
    uint32_t GetSize() const { return m_Size; }

private:
    SDL_GPUDevice* m_Device;
    SDL_GPUBuffer* m_Buffer;
    uint32_t m_Size;
    BufferUsage m_Usage;
};
