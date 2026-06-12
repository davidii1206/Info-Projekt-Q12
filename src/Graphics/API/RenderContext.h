/**
 * @file RenderContext.h
 * @brief Command recording context for graphics and compute tasks.
 */
#pragma once
#include <SDL3/SDL_gpu.h>
#include "GraphicsPipeline.h"
#include "ComputePipeline.h"
#include "GPUBuffer.h"
#include "Texture.h"

/**
 * @class RenderContext
 * @brief Simplified command recorder for both Graphics and Compute tasks.
 * 
 * This class provides a high-level interface for recording GPU commands 
 * into a command buffer, abstracting away some of the lower-level SDL3 GPU API.
 */
class RenderContext {
public:
    /**
     * @brief Constructs a RenderContext for graphics tasks.
     * @param pass Pointer to the active SDL_GPURenderPass.
     * @param cmd Pointer to the active SDL_GPUCommandBuffer.
     */
    RenderContext(SDL_GPURenderPass* pass, SDL_GPUCommandBuffer* cmd) 
        : m_RenderPass(pass), m_ComputePass(nullptr), m_Cmd(cmd) {}

    /**
     * @brief Constructs a RenderContext for compute tasks.
     * @param pass Pointer to the active SDL_GPUComputePass.
     * @param cmd Pointer to the active SDL_GPUCommandBuffer.
     */
    RenderContext(SDL_GPUComputePass* pass, SDL_GPUCommandBuffer* cmd) 
        : m_RenderPass(nullptr), m_ComputePass(pass), m_Cmd(cmd) {}

    // --- Graphics Commands ---

    /**
     * @brief Binds a graphics pipeline state object for subsequent draw calls.
     * @param pipeline The graphics pipeline to bind.
     */
    void BindPipeline(GraphicsPipeline* pipeline);

    /**
     * @brief Binds a vertex buffer to a specific input slot.
     * @param buffer The GPU buffer containing vertex data.
     * @param slot The binding slot index (default 0).
     */
    void BindVertexBuffer(GPUBuffer* buffer, uint32_t slot = 0);

    /**
     * @brief Binds an index buffer for indexed drawing.
     * @param buffer The GPU buffer containing indices.
     * @param indexSize The size of each index (16-bit or 32-bit).
     */
    void BindIndexBuffer(GPUBuffer* buffer, SDL_GPUIndexElementSize indexSize = SDL_GPU_INDEXELEMENTSIZE_32BIT);

    /**
     * @brief Binds a texture and sampler to a fragment shader slot.
     * @param slot The binding slot index in the shader.
     * @param texture The texture to bind.
     */
    void BindFragmentTexture(uint32_t slot, Texture* texture);

    /**
     * @brief Pushes small constant data to the vertex shader.
     * @param slot The binding slot index.
     * @param data Pointer to the data.
     * @param size Size of the data in bytes.
     */
    void PushVertexConstants(uint32_t slot, const void* data, uint32_t size);

    /**
     * @brief Pushes small constant data to the fragment shader.
     * @param slot The binding slot index.
     * @param data Pointer to the data.
     * @param size Size of the data in bytes.
     */
    void PushFragmentConstants(uint32_t slot, const void* data, uint32_t size);

    /**
     * @brief Draws primitives using the bound pipeline and vertex buffer.
     * @param vertexCount Number of vertices to draw.
     * @param instanceCount Number of instances to draw (default 1).
     * @param firstVertex Index of the first vertex to draw.
     * @param firstInstance Index of the first instance.
     */
    void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);

    /**
     * @brief Draws primitives using the bound index buffer.
     * @param indexCount Number of indices to draw.
     * @param instanceCount Number of instances to draw (default 1).
     * @param firstIndex Index of the first index in the buffer.
     * @param vertexOffset Value added to each index before fetching vertices.
     * @param firstInstance Index of the first instance.
     */
    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0);

    /**
     * @brief Issues an indirect draw call.
     * @param buffer The GPU buffer containing the draw parameters.
     * @param offset The offset in the buffer where parameters start.
     * @param drawCount The number of draws to issue.
     */
    void DrawIndirect(GPUBuffer* buffer, uint32_t offset = 0, uint32_t drawCount = 1);

    /**
     * @brief Issues an indirect indexed draw call.
     * @param buffer The GPU buffer containing the draw parameters.
     * @param offset The offset in the buffer where parameters start.
     * @param drawCount The number of draws to issue.
     */
    void DrawIndexedIndirect(GPUBuffer* buffer, uint32_t offset, uint32_t drawCount);

    /**
     * @brief Binds a storage buffer for read-only access in a vertex shader.
     * @param slot The binding slot index.
     * @param buffer The GPU buffer to bind.
     */
    void BindVertexStorageBuffer(uint32_t slot, GPUBuffer* buffer);

    /**
     * @brief Binds a storage buffer for read-only access in a fragment shader.
     * @param slot The binding slot index.
     * @param buffer The GPU buffer to bind.
     */
    void BindFragmentStorageBuffer(uint32_t slot, GPUBuffer* buffer);

    /**
     * @brief Binds a storage buffer for read-only access in both vertex and fragment shaders.
     * @param slot The binding slot index.
     * @param buffer The GPU buffer to bind.
     */
    void BindGraphicsStorageBuffer(uint32_t slot, GPUBuffer* buffer);

    // --- Compute Commands ---

    /**
     * @brief Binds a compute pipeline for subsequent dispatch calls.
     * @param pipeline The compute pipeline to bind.
     */
    void BindComputePipeline(ComputePipeline* pipeline);

    /**
     * @brief Binds a storage buffer for read/write access in a compute shader.
     * @param slot The binding slot index.
     * @param buffer The GPU buffer to bind.
     * @param write If true, allows writing to the buffer (Read-Write).
     */
    void BindComputeStorageBuffer(uint32_t slot, GPUBuffer* buffer, bool write = false);

    /**
     * @brief Binds a storage texture for read/write access in a compute shader.
     * @param slot The binding slot index.
     * @param texture The GPU texture to bind.
     * @param write If true, allows writing to the texture (Read-Write).
     */
    void BindComputeStorageTexture(uint32_t slot, Texture* texture, bool write = false);

    /**
     * @brief Dispatches a compute workgroup.
     * @param groupCountX Number of workgroups in the X dimension.
     * @param groupCountY Number of workgroups in the Y dimension.
     * @param groupCountZ Number of workgroups in the Z dimension.
     */
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

private:
    SDL_GPURenderPass* m_RenderPass;   /**< Pointer to the active render pass. */
    SDL_GPUComputePass* m_ComputePass; /**< Pointer to the active compute pass. */
    SDL_GPUCommandBuffer* m_Cmd;       /**< Pointer to the active command buffer. */
};
