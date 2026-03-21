#include "Framebuffer.h"
#include <spdlog/spdlog.h>

Framebuffer::Framebuffer(SDL_GPUDevice* device, uint32_t width, uint32_t height, const std::vector<SDL_GPUTextureFormat>& formats, bool hasDepth)
    : m_Device(device), m_Width(width), m_Height(height)
{
    // 1. Create Color Targets
    for (const auto& format : formats) {
        auto tex = std::make_unique<Texture>(
            m_Device, 
            m_Width, 
            m_Height, 
            format, 
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
            TextureFilter::Nearest
        );
        m_ColorTargets.push_back(std::move(tex));
    }

    // 2. Create Depth Target if requested
    if (hasDepth) {
        SDL_GPUTextureFormat depthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        
        m_DepthTarget = std::make_unique<Texture>(
            m_Device,
            m_Width,
            m_Height,
            depthFormat,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
            TextureFilter::Nearest
        );
    }

    spdlog::info("Framebuffer created ({}x{}) with {} color targets", m_Width, m_Height, m_ColorTargets.size());
}

Framebuffer::~Framebuffer() {
    // std::unique_ptr handles Texture destruction
}
