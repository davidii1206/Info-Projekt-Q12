#include "ComputePipeline.h"
#include <spdlog/spdlog.h>

ComputePipeline::ComputePipeline(SDL_GPUDevice* device, const ComputeConfig& config)
    : m_Device(device), m_Pipeline(nullptr)
{
    SDL_GPUComputePipelineCreateInfo createInfo = {};
    createInfo.code_size = config.codeSize;
    createInfo.code = config.code;
    createInfo.entrypoint = config.entrypoint;
    createInfo.format = config.format;

    createInfo.num_samplers = config.numSamplers;
    createInfo.num_readonly_storage_textures = config.numReadonlyStorageTextures;
    createInfo.num_readonly_storage_buffers = config.numReadonlyStorageBuffers;
    createInfo.num_readwrite_storage_textures = config.numReadwriteStorageTextures;
    createInfo.num_readwrite_storage_buffers = config.numReadwriteStorageBuffers;
    createInfo.num_uniform_buffers = config.numUniformBuffers;

    createInfo.threadcount_x = config.threadCountX;
    createInfo.threadcount_y = config.threadCountY;
    createInfo.threadcount_z = config.threadCountZ;

    m_Pipeline = SDL_CreateGPUComputePipeline(m_Device, &createInfo);
    if (!m_Pipeline) {
        spdlog::error("Failed to create Compute Pipeline: {}", SDL_GetError());
    }
}

ComputePipeline::~ComputePipeline() {
    if (m_Pipeline) {
        SDL_ReleaseGPUComputePipeline(m_Device, m_Pipeline);
    }
}
