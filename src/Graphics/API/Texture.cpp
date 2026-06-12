/**
 * @file Texture.cpp
 * @brief Implementation of the Texture class for GPU texture and sampler management.
 */
#include "Texture.h"
#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>
#include <stb_image.h>

/**
 * @brief Internal helper to set up SDL_GPUSamplerCreateInfo.
 * @param samplerDesc Reference to the creation info structure.
 * @param filter The desired texture filter mode.
 */
void SetupSampler(SDL_GPUSamplerCreateInfo& samplerDesc, TextureFilter filter) {
    samplerDesc.min_filter = (filter == TextureFilter::Linear) ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
    samplerDesc.mag_filter = (filter == TextureFilter::Linear) ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
    samplerDesc.mipmap_mode = (filter == TextureFilter::Linear) ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samplerDesc.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerDesc.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerDesc.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerDesc.max_anisotropy = 1.0f;
    samplerDesc.min_lod = 0.0f;
    samplerDesc.max_lod = 1000.0f;
}

Texture::Texture(SDL_GPUDevice* device, const std::string& filePath, TextureFilter filter)
    : m_Device(device), m_Texture(nullptr), m_Sampler(nullptr), m_Width(0), m_Height(0) 
{
    int width, height, channels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* pixels = stbi_load(filePath.c_str(), &width, &height, &channels, 4);
    
    if (!pixels) {
        spdlog::error("Failed to load image: {} - {}", filePath, stbi_failure_reason());
        return;
    }

    m_Width = (uint32_t)width;
    m_Height = (uint32_t)height;

    // Create GPU texture description
    SDL_GPUTextureCreateInfo textureDesc = {};
    textureDesc.type = SDL_GPU_TEXTURETYPE_2D;
    textureDesc.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    textureDesc.width = m_Width;
    textureDesc.height = m_Height;
    textureDesc.layer_count_or_depth = 1;
    textureDesc.num_levels = 1;
    textureDesc.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    m_Texture = SDL_CreateGPUTexture(m_Device, &textureDesc);
    if (!m_Texture) {
        spdlog::error("Failed to create GPU Texture for {}: {}", filePath, SDL_GetError());
        stbi_image_free(pixels);
        return;
    }

    // Use a staging buffer to upload pixel data
    SDL_GPUTransferBufferCreateInfo stagingDesc = {};
    stagingDesc.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    stagingDesc.size = m_Width * m_Height * 4;
    
    SDL_GPUTransferBuffer* stagingBuffer = SDL_CreateGPUTransferBuffer(m_Device, &stagingDesc);
    void* mappedData = SDL_MapGPUTransferBuffer(m_Device, stagingBuffer, false);
    std::memcpy(mappedData, pixels, stagingDesc.size);
    SDL_UnmapGPUTransferBuffer(m_Device, stagingBuffer);

    // Record and submit the upload command
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_Device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo source = {};
    source.transfer_buffer = stagingBuffer;
    source.offset = 0;

    SDL_GPUTextureRegion destination = {};
    destination.texture = m_Texture;
    destination.w = m_Width;
    destination.h = m_Height;
    destination.d = 1;

    SDL_UploadToGPUTexture(copyPass, &source, &destination, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmd);

    // Clean up temporary resources
    SDL_ReleaseGPUTransferBuffer(m_Device, stagingBuffer);
    stbi_image_free(pixels);

    // Create the sampler
    SDL_GPUSamplerCreateInfo samplerDesc = {};
    SetupSampler(samplerDesc, filter);
    m_Sampler = SDL_CreateGPUSampler(m_Device, &samplerDesc);
    
    spdlog::info("Texture loaded successfully: {} ({}x{})", filePath, m_Width, m_Height);
}

Texture::Texture(SDL_GPUDevice* device, unsigned char* pixels, uint32_t width, uint32_t height, TextureFilter filter)
    : m_Device(device), m_Texture(nullptr), m_Sampler(nullptr), m_Width(width), m_Height(height)
{
    SDL_GPUTextureCreateInfo textureDesc = {};
    textureDesc.type = SDL_GPU_TEXTURETYPE_2D;
    textureDesc.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    textureDesc.width = m_Width;
    textureDesc.height = m_Height;
    textureDesc.layer_count_or_depth = 1;
    textureDesc.num_levels = 1;
    textureDesc.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    m_Texture = SDL_CreateGPUTexture(m_Device, &textureDesc);
    if (!m_Texture) {
        spdlog::error("Failed to create GPU Texture from raw data ({}x{}): {}", m_Width, m_Height, SDL_GetError());
        return;
    }

    SDL_GPUTransferBufferCreateInfo stagingDesc = {};
    stagingDesc.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    stagingDesc.size = m_Width * m_Height * 4;
    
    SDL_GPUTransferBuffer* stagingBuffer = SDL_CreateGPUTransferBuffer(m_Device, &stagingDesc);
    void* mappedData = SDL_MapGPUTransferBuffer(m_Device, stagingBuffer, false);
    std::memcpy(mappedData, pixels, stagingDesc.size);
    SDL_UnmapGPUTransferBuffer(m_Device, stagingBuffer);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_Device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo source = {};
    source.transfer_buffer = stagingBuffer;
    source.offset = 0;

    SDL_GPUTextureRegion destination = {};
    destination.texture = m_Texture;
    destination.w = m_Width;
    destination.h = m_Height;
    destination.d = 1;

    SDL_UploadToGPUTexture(copyPass, &source, &destination, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmd);

    SDL_ReleaseGPUTransferBuffer(m_Device, stagingBuffer);

    SDL_GPUSamplerCreateInfo samplerDesc = {};
    SetupSampler(samplerDesc, filter);
    m_Sampler = SDL_CreateGPUSampler(m_Device, &samplerDesc);
}

Texture::Texture(SDL_GPUDevice* device, uint32_t width, uint32_t height, SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage, TextureFilter filter)
    : m_Device(device), m_Texture(nullptr), m_Sampler(nullptr), m_Width(width), m_Height(height)
{
    SDL_GPUTextureCreateInfo textureDesc = {};
    textureDesc.type = SDL_GPU_TEXTURETYPE_2D;
    textureDesc.format = format;
    textureDesc.width = m_Width;
    textureDesc.height = m_Height;
    textureDesc.layer_count_or_depth = 1;
    textureDesc.num_levels = 1;
    textureDesc.usage = usage;

    m_Texture = SDL_CreateGPUTexture(m_Device, &textureDesc);
    if (!m_Texture) {
        spdlog::error("Failed to create empty GPU Texture ({}x{}): {}", m_Width, m_Height, SDL_GetError());
        return;
    }

    if (usage & SDL_GPU_TEXTUREUSAGE_SAMPLER) {
        SDL_GPUSamplerCreateInfo samplerDesc = {};

        if (filter == TextureFilter::ShadowCompare) {
            // Depth compare sampler for sampler2DShadow (PCF shadow mapping)
            samplerDesc.min_filter     = SDL_GPU_FILTER_LINEAR;
            samplerDesc.mag_filter     = SDL_GPU_FILTER_LINEAR;
            samplerDesc.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            samplerDesc.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            samplerDesc.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            samplerDesc.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            samplerDesc.enable_compare = true;
            samplerDesc.compare_op     = SDL_GPU_COMPAREOP_LESS_OR_EQUAL; // FIX: LESS misses equal-depth fragments on flat surfaces
            samplerDesc.min_lod        = 0.0f;
            samplerDesc.max_lod        = 1.0f;
        } else {
            SetupSampler(samplerDesc, filter);
            samplerDesc.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            samplerDesc.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            samplerDesc.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        }

        m_Sampler = SDL_CreateGPUSampler(m_Device, &samplerDesc);
    }
}

Texture::~Texture() {
    if (m_Sampler) SDL_ReleaseGPUSampler(m_Device, m_Sampler);
    if (m_Texture) SDL_ReleaseGPUTexture(m_Device, m_Texture);
}
