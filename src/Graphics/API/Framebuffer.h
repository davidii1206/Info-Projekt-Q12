/**
 * @file Framebuffer.h
 * @brief Framebuffer management for off-screen rendering.
 */
#pragma once
#include <SDL3/SDL_gpu.h>
#include "Texture.h"
#include <vector>
#include <memory>

/**
 * @class Framebuffer
 * @brief Manages a set of render target textures and an optional depth buffer.
 * 
 * Used for off-screen rendering, such as G-Buffers for deferred shading
 * or post-processing effects.
 */
class Framebuffer {
public:
    /**
     * @brief Creates a framebuffer with specific dimensions.
     * @param device Pointer to the active SDL_GPUDevice.
     * @param width Width in pixels.
     * @param height Height in pixels.
     * @param formats List of pixel formats for each color target.
     * @param hasDepth Whether to include a depth target.
     * @param depthFilter Sampling filter to use for the depth texture.
     */
    Framebuffer(SDL_GPUDevice* device, uint32_t width, uint32_t height, const std::vector<SDL_GPUTextureFormat>& formats, bool hasDepth = true, TextureFilter depthFilter = TextureFilter::Nearest);

    /**
     * @brief Destroys the framebuffer and releases all target textures.
     */
    ~Framebuffer();

    /**
     * @brief Returns the color target at a specific index.
     * @param index Index of the color target.
     * @return Pointer to the Texture.
     */
    Texture* GetColorTarget(uint32_t index) const { return m_ColorTargets[index].get(); }

    /**
     * @brief Returns the depth target texture.
     * @return Pointer to the depth Texture, or nullptr if none.
     */
    Texture* GetDepthTarget() const { return m_DepthTarget.get(); }

    /**
     * @brief Returns the total number of color targets.
     * @return Number of color targets.
     */
    uint32_t GetColorTargetCount() const { return (uint32_t)m_ColorTargets.size(); }

    /**
     * @brief Gets the width of the framebuffer.
     * @return Width in pixels.
     */
    uint32_t GetWidth() const { return m_Width; }

    /**
     * @brief Gets the height of the framebuffer.
     * @return Height in pixels.
     */
    uint32_t GetHeight() const { return m_Height; }

private:
    SDL_GPUDevice* m_Device;                           /**< Pointer to the SDL GPU device. */
    uint32_t m_Width;                                  /**< Width of the framebuffer in pixels. */
    uint32_t m_Height;                                 /**< Height of the framebuffer in pixels. */
    std::vector<std::unique_ptr<Texture>> m_ColorTargets; /**< List of color target textures. */
    std::unique_ptr<Texture> m_DepthTarget;            /**< Optional depth target texture. */
};
