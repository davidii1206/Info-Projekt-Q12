#pragma once
#include <SDL3/SDL_gpu.h>
#include <string>
#include <vector>

/**
 * @enum ShaderStage
 * @brief Defines which stage of the graphics pipeline the shader belongs to.
 */
enum class ShaderStage {
    Vertex,
    Fragment
};

/**
 * @struct ShaderResourceLayout
 * @brief Describes the resources used by a shader.
 * 
 * SDL3 GPU requires knowing exactly how many resources a shader will bind
 * at creation time.
 */
struct ShaderResourceLayout {
    uint32_t numSamplers = 0;
    uint32_t numStorageTextures = 0;
    uint32_t numStorageBuffers = 0;
    uint32_t numUniformBuffers = 0;
};

/**
 * @class Shader
 * @brief Wrapper for SDL_GPUShader objects.
 * 
 * Handles loading binary shader code (SPIR-V, DXBC, or MSL) from disk
 * and creating the GPU-side shader module.
 */
class Shader {
public:
    /**
     * @brief Loads and creates a shader from a binary file.
     * @param device Pointer to the active SDL_GPUDevice.
     * @param filePath Path to the binary shader file.
     * @param stage Whether this is a vertex or fragment shader.
     * @param layout Description of the resources this shader uses.
     */
    Shader(SDL_GPUDevice* device, const std::string& filePath, ShaderStage stage, const ShaderResourceLayout& layout);
    ~Shader();

    /** @brief Gets the native SDL shader handle. */
    SDL_GPUShader* GetHandle() const { return m_Shader; }
    
    /** @brief Gets the shader stage. */
    ShaderStage GetStage() const { return m_Stage; }

private:
    SDL_GPUDevice* m_Device;
    SDL_GPUShader* m_Shader;
    ShaderStage m_Stage;
};
