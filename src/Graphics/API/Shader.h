/**
 * @file Shader.h
 * @brief Shader management and resource layout for the GPU.
 */
#pragma once
#include <SDL3/SDL_gpu.h>
#include <string>
#include <vector>

/**
 * @enum ShaderStage
 * @brief Defines which stage of the graphics pipeline the shader belongs to.
 */
enum class ShaderStage {
    Vertex,   /**< Vertex shader stage. */
    Fragment  /**< Fragment shader stage. */
};

/**
 * @struct ShaderResourceLayout
 * @brief Describes the resources used by a shader.
 * 
 * SDL3 GPU requires knowing exactly how many resources a shader will bind
 * at creation time.
 */
struct ShaderResourceLayout {
    uint32_t numSamplers = 0;        /**< Number of samplers used by the shader. */
    uint32_t numStorageTextures = 0; /**< Number of storage textures used by the shader. */
    uint32_t numStorageBuffers = 0;  /**< Number of storage buffers used by the shader. */
    uint32_t numUniformBuffers = 0;  /**< Number of uniform buffers used by the shader. */
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
     *        The correct file extension (.spv / .dxbc / .dxil / .msl) is
     *        appended automatically based on the device's shader format.
     * @param device Pointer to the active SDL_GPUDevice.
     * @param filePath Path to the shader file without format extension
     *        (e.g. "shaders/model.vert" loads "model.vert.spv" on Vulkan).
     * @param stage Whether this is a vertex or fragment shader.
     * @param layout Description of the resources this shader uses.
     */
    Shader(SDL_GPUDevice* device, const std::string& filePath, ShaderStage stage, const ShaderResourceLayout& layout);

    /**
     * @brief Destroys the shader and releases GPU resources.
     */
    ~Shader();

    /**
     * @brief Gets the native SDL shader handle.
     * @return Pointer to SDL_GPUShader.
     */
    SDL_GPUShader* GetHandle() const { return m_Shader; }
    
    /**
     * @brief Gets the shader stage.
     * @return The ShaderStage.
     */
    ShaderStage GetStage() const { return m_Stage; }

private:
    SDL_GPUDevice* m_Device; /**< Pointer to the SDL GPU device. */
    SDL_GPUShader* m_Shader; /**< Native SDL shader handle. */
    ShaderStage m_Stage;     /**< The stage this shader is for. */
};
