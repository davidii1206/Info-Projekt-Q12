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
    // Load binary file from disk
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open shader file: {}", filePath);
        return;
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<uint8_t> buffer(fileSize);
    file.seekg(0);
    file.read((char*)buffer.data(), fileSize);
    file.close();

    // Determine shader format based on device capabilities
    SDL_GPUShaderFormat format = SDL_GetGPUShaderFormats(m_Device);
    SDL_GPUShaderFormat chosenFormat = SDL_GPU_SHADERFORMAT_INVALID;

    // Preference: SPIRV (Vulkan) -> DXBC (D3D11) -> MSL (Metal)
    if (format & SDL_GPU_SHADERFORMAT_SPIRV) {
        chosenFormat = SDL_GPU_SHADERFORMAT_SPIRV;
    } else if (format & SDL_GPU_SHADERFORMAT_DXBC) {
        chosenFormat = SDL_GPU_SHADERFORMAT_DXBC;
    } else if (format & SDL_GPU_SHADERFORMAT_MSL) {
        chosenFormat = SDL_GPU_SHADERFORMAT_MSL;
    }

    // Set up shader creation description
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
    if (!m_Shader) {
        spdlog::error("Failed to create GPU Shader from {}: {}", filePath, SDL_GetError());
    } else {
        spdlog::info("Shader created successfully: {}", filePath);
    }
}

Shader::~Shader() {
    if (m_Shader) {
        SDL_ReleaseGPUShader(m_Device, m_Shader);
    }
}
