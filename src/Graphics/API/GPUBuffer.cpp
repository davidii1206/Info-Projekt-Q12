/**
 * @file GPUBuffer.cpp
 * @brief Implementation of the GPUBuffer class for managing GPU memory.
 */
#include "GPUBuffer.h"
#include <spdlog/spdlog.h>
#include <cstring>

GPUBuffer::GPUBuffer(SDL_GPUDevice* device, BufferUsage usage, uint32_t size)
    : m_Device(device), m_Size(size), m_Usage(usage) 
{
    SDL_GPUBufferCreateInfo desc = {};
    desc.size = size;
    
    // Set appropriate usage flags based on BufferUsage enum
    switch (usage) {
        case BufferUsage::Vertex:  desc.usage = SDL_GPU_BUFFERUSAGE_VERTEX; break;
        case BufferUsage::Index:   desc.usage = SDL_GPU_BUFFERUSAGE_INDEX; break;
        case BufferUsage::Uniform: desc.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ; break;
        case BufferUsage::Compute: desc.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE; break;
    }

    m_Buffer = SDL_CreateGPUBuffer(device, &desc);
    if (!m_Buffer) {
        spdlog::error("Failed to create GPU Buffer: {}", SDL_GetError());
    }
}

GPUBuffer::~GPUBuffer() {
    if (m_Buffer) {
        SDL_ReleaseGPUBuffer(m_Device, m_Buffer);
    }
}

void GPUBuffer::Upload(const void* data, uint32_t size, uint32_t offset, SDL_GPUCommandBuffer* cmd, bool cycle) {
    if (size + offset > m_Size) {
        spdlog::error("GPUBuffer::Upload: Attempted to upload more data than buffer size!");
        return;
    }

    // Create a temporary staging buffer for the transfer
    SDL_GPUTransferBufferCreateInfo stagingDesc = {};
    stagingDesc.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    stagingDesc.size = size;
    
    SDL_GPUTransferBuffer* stagingBuffer = SDL_CreateGPUTransferBuffer(m_Device, &stagingDesc);
    if (!stagingBuffer) {
        spdlog::error("Failed to create staging buffer: {}", SDL_GetError());
        return;
    }

    // Map and copy data from CPU memory to staging buffer
    void* mappedData = SDL_MapGPUTransferBuffer(m_Device, stagingBuffer, false);
    if (!mappedData) {
        spdlog::error("GPUBuffer::Upload: Failed to map transfer buffer: {}", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(m_Device, stagingBuffer);
        return;
    }
    std::memcpy(mappedData, data, size);
    SDL_UnmapGPUTransferBuffer(m_Device, stagingBuffer);

    // Record the copy command into the provided or internal command buffer
    bool submitInternal = false;
    if (!cmd) {
        cmd = SDL_AcquireGPUCommandBuffer(m_Device);
        submitInternal = true;
    }

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);
    
    SDL_GPUTransferBufferLocation source = { stagingBuffer, 0 };
    SDL_GPUBufferRegion destination = { m_Buffer, offset, size };
    
    SDL_UploadToGPUBuffer(copyPass, &source, &destination, cycle);
    
    SDL_EndGPUCopyPass(copyPass);
    
    if (submitInternal) {
        SDL_SubmitGPUCommandBuffer(cmd);
    }

    // Clean up staging resources
    SDL_ReleaseGPUTransferBuffer(m_Device, stagingBuffer);
}
