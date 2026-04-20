/**
 * @file RenderContext.cpp
 * @brief Implementation of the RenderContext class for recording GPU commands.
 */
#include "RenderContext.h"
#include <spdlog/spdlog.h>

// --- Graphics ---

void RenderContext::BindPipeline(GraphicsPipeline* pipeline) {
    if (m_RenderPass) SDL_BindGPUGraphicsPipeline(m_RenderPass, pipeline->GetHandle());
}

void RenderContext::BindVertexBuffer(GPUBuffer* buffer, uint32_t slot) {
    if (m_RenderPass) {
        SDL_GPUBufferBinding binding = { buffer->GetHandle(), 0 };
        SDL_BindGPUVertexBuffers(m_RenderPass, slot, &binding, 1);
    }
}

void RenderContext::BindIndexBuffer(GPUBuffer* buffer, SDL_GPUIndexElementSize indexSize) {
    if (m_RenderPass) {
        SDL_GPUBufferBinding binding = { buffer->GetHandle(), 0 };
        SDL_BindGPUIndexBuffer(m_RenderPass, &binding, indexSize);
    }
}

void RenderContext::BindFragmentTexture(uint32_t slot, Texture* texture) {
    if (m_RenderPass && texture) {
        SDL_GPUTextureSamplerBinding binding = { texture->GetHandle(), texture->GetSampler() };
        if (!binding.sampler) spdlog::error("RenderContext: Binding texture to slot {} with NULL sampler!", slot);
        
        // Bind the combined sampler (required for SPIR-V sampler2D)
        SDL_BindGPUFragmentSamplers(m_RenderPass, slot, &binding, 1);
    }
}

void RenderContext::PushVertexConstants(uint32_t slot, const void* data, uint32_t size) {
    SDL_PushGPUVertexUniformData(m_Cmd, slot, data, size);
}

void RenderContext::PushFragmentConstants(uint32_t slot, const void* data, uint32_t size) {
    SDL_PushGPUFragmentUniformData(m_Cmd, slot, data, size);
}

void RenderContext::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    if (m_RenderPass) SDL_DrawGPUPrimitives(m_RenderPass, vertexCount, instanceCount, firstVertex, firstInstance);
}

void RenderContext::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    if (m_RenderPass) SDL_DrawGPUIndexedPrimitives(m_RenderPass, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void RenderContext::DrawIndirect(GPUBuffer* buffer, uint32_t offset, uint32_t drawCount) {
    if (m_RenderPass) SDL_DrawGPUPrimitivesIndirect(m_RenderPass, buffer->GetHandle(), offset, drawCount);
}

void RenderContext::DrawIndexedIndirect(GPUBuffer* buffer, uint32_t offset, uint32_t drawCount) {
    if (m_RenderPass) SDL_DrawGPUIndexedPrimitivesIndirect(m_RenderPass, buffer->GetHandle(), offset, drawCount);
}

void RenderContext::BindVertexStorageBuffer(uint32_t slot, GPUBuffer* buffer) {
    if (m_RenderPass && buffer) {
        SDL_GPUBuffer* handle = buffer->GetHandle();
        SDL_BindGPUVertexStorageBuffers(m_RenderPass, slot, &handle, 1);
    }
}

void RenderContext::BindFragmentStorageBuffer(uint32_t slot, GPUBuffer* buffer) {
    if (m_RenderPass && buffer) {
        SDL_GPUBuffer* handle = buffer->GetHandle();
        SDL_BindGPUFragmentStorageBuffers(m_RenderPass, slot, &handle, 1);
    }
}

void RenderContext::BindGraphicsStorageBuffer(uint32_t slot, GPUBuffer* buffer) {
    if (m_RenderPass && buffer) {
        SDL_GPUBuffer* handle = buffer->GetHandle();
        SDL_BindGPUVertexStorageBuffers(m_RenderPass, slot, &handle, 1);
        SDL_BindGPUFragmentStorageBuffers(m_RenderPass, slot, &handle, 1);
    }
}

// --- Compute ---

void RenderContext::BindComputePipeline(ComputePipeline* pipeline) {
    if (m_ComputePass) SDL_BindGPUComputePipeline(m_ComputePass, pipeline->GetHandle());
}

void RenderContext::BindComputeStorageBuffer(uint32_t slot, GPUBuffer* buffer, bool write) {
    if (m_ComputePass) {
        SDL_GPUBuffer* handle = buffer->GetHandle();
        SDL_BindGPUComputeStorageBuffers(m_ComputePass, slot, &handle, 1);
    }
}

void RenderContext::BindComputeStorageTexture(uint32_t slot, Texture* texture, bool write) {
    if (m_ComputePass) {
        SDL_GPUTexture* handle = texture->GetHandle();
        SDL_BindGPUComputeStorageTextures(m_ComputePass, slot, &handle, 1);
    }
}

void RenderContext::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    if (m_ComputePass) SDL_DispatchGPUCompute(m_ComputePass, groupCountX, groupCountY, groupCountZ);
}
