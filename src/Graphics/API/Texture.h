#pragma once
#include <SDL3/SDL_gpu.h>
#include <string>

/**
 * @enum TextureFilter
 * @brief Defines how the texture is sampled when scaled.
 */
enum class TextureFilter {
    Nearest, ///< Pixelated/Blocky filtering.
    Linear   ///< Smooth/Blurred filtering.
};

/**
 * @class Texture
 * @brief Wrapper for SDL_GPUTexture and SDL_GPUSampler.
 * 
 * Handles loading image files from disk (via stb_image) and 
 * uploading them to GPU memory.
 */
class Texture {
public:
    /**
     * @brief Loads a texture from an image file.
     * @param device Pointer to the active SDL_GPUDevice.
     * @param filePath Path to the image file (PNG, JPG, etc.).
     * @param filter The filtering mode to use for this texture.
     */
    Texture(SDL_GPUDevice* device, const std::string& filePath, TextureFilter filter = TextureFilter::Linear);

    /**
     * @brief Creates a texture from raw pixel data in memory.
     * @param device Pointer to the active SDL_GPUDevice.
     * @param pixels Pointer to the RGBA8 pixel data.
     * @param width Width in pixels.
     * @param height Height in pixels.
     * @param filter The filtering mode to use.
     */
    Texture(SDL_GPUDevice* device, unsigned char* pixels, uint32_t width, uint32_t height, TextureFilter filter = TextureFilter::Linear);

    /**
     * @brief Creates an empty GPU texture with specific parameters.
     * @param device Pointer to the active SDL_GPUDevice.
     * @param width Width in pixels.
     * @param height Height in pixels.
     * @param format SDL GPU texture format.
     * @param usage SDL GPU texture usage flags.
     * @param filter The filtering mode to use.
     */
    Texture(SDL_GPUDevice* device, uint32_t width, uint32_t height, SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage, TextureFilter filter = TextureFilter::Linear);

    ~Texture();

    /** @brief Gets the native SDL texture handle. */
    SDL_GPUTexture* GetHandle() const { return m_Texture; }

    /** @brief Gets the associated SDL sampler handle. */
    SDL_GPUSampler* GetSampler() const { return m_Sampler; }

    /** @brief Gets the width of the texture in pixels. */
    uint32_t GetWidth() const { return m_Width; }

    /** @brief Gets the height of the texture in pixels. */
    uint32_t GetHeight() const { return m_Height; }

private:
    SDL_GPUDevice* m_Device;
    SDL_GPUTexture* m_Texture;
    SDL_GPUSampler* m_Sampler;
    uint32_t m_Width;
    uint32_t m_Height;
};
