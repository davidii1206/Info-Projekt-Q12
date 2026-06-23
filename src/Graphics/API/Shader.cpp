/**
 * @file Shader.cpp
 * @brief Implementation of the Shader class for loading and creating GPU shaders.
 */
#include "Shader.h"
#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>
#include <fstream>

Shader::Shader(SDL_GPUDevice* device, const std::string& filePath, ShaderStage stage, const ShaderResourceLayout& layout)
    : m_Device(device), m_Shader(nullptr), m_Stage(stage) 
{
    SDL_GPUShaderFormat supportedFormats = SDL_GetGPUShaderFormats(m_Device);

    // Ordered preference: try each available format until we find a file that exists
    struct FormatEntry {
        SDL_GPUShaderFormat format;
        const char* ext;
    };
    FormatEntry candidates[] = {
        {SDL_GPU_SHADERFORMAT_SPIRV, ".spv"},
        {SDL_GPU_SHADERFORMAT_DXBC,  ".dxbc"},
        {SDL_GPU_SHADERFORMAT_DXIL,  ".dxil"},
        {SDL_GPU_SHADERFORMAT_MSL,   ".msl"},
    };

    SDL_GPUShaderFormat chosenFormat = SDL_GPU_SHADERFORMAT_INVALID;
    std::string actualPath;

    for (const auto& candidate : candidates) {
        if (!(supportedFormats & candidate.format))
            continue;
        actualPath = filePath + candidate.ext;
        std::ifstream file(actualPath, std::ios::ate | std::ios::binary);
        if (!file.is_open())
            continue;

        // File exists — read it and use this format
        chosenFormat = candidate.format;
        size_t fileSize = (size_t)file.tellg();
        std::vector<uint8_t> buffer(fileSize);
        file.seekg(0);
        file.read((char*)buffer.data(), fileSize);
        file.close();

        SDL_GPUShaderCreateInfo desc = {};
        desc.code_size = fileSize;
        desc.code = buffer.data();
        desc.entrypoint = "main";
        desc.format = chosenFormat;
        desc.stage = (stage == ShaderStage::Vertex) ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
        desc.num_samplers = layout.numSamplers;
        desc.num_storage_textures = layout.numStorageTextures;
        desc.num_storage_buffers = layout.numStorageBuffers;
        desc.num_uniform_buffers = layout.numUniformBuffers;

        m_Shader = SDL_CreateGPUShader(m_Device, &desc);
        if (m_Shader) {
            spdlog::info("Shader created successfully: {}", actualPath);
            return;
        }
        spdlog::error("Failed to create GPU Shader from {}: {}", actualPath, SDL_GetError());
        // File existed but SDL rejected it — don't retry with other formats
        return;
    }

    spdlog::error("Failed to open shader file for {} (tried: spv, dxbc, dxil, msl)", filePath);
}

Shader::~Shader() {
    if (m_Shader) {
        SDL_ReleaseGPUShader(m_Device, m_Shader);
    }
}
