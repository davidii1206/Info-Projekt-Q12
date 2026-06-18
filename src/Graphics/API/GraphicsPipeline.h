/**
 * @file GraphicsPipeline.h
 * @brief Graphics pipeline state management for the GPU.
 */
#pragma once
#include <SDL3/SDL_gpu.h>
#include "Shader.h"
#include <vector>

/**
 * @struct VertexAttribute
 * @brief Describes a single attribute in a vertex (e.g., Position, Color).
 */
struct VertexAttribute {
    uint32_t location;      /**< Shader location (layout(location = X)). */
    SDL_GPUVertexElementFormat format; /**< Data format (e.g., Float3, UB4). */
    uint32_t offset;        /**< Offset in bytes from the start of the vertex struct. */
};

/**
 * @struct PipelineConfig
 * @brief Configuration for creating a Graphics Pipeline.
 */
struct PipelineConfig {
    /** @brief The vertex shader module to use. */
    Shader* vertexShader = nullptr;
    
    /** @brief The fragment shader module to use. */
    Shader* fragmentShader = nullptr;
    
    /** @brief List of vertex attributes (Position, Normal, etc.). */
    std::vector<VertexAttribute> vertexAttributes;
    
    /** @brief Stride in bytes between consecutive vertices in the buffer. */
    uint32_t vertexStride = 0;
    
    /** @brief Type of primitives to draw (TriangleList, LineList, etc.). */
    SDL_GPUPrimitiveType primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    
    /** @brief Fill mode (Fill or Line/Wireframe). */
    SDL_GPUFillMode fillMode = SDL_GPU_FILLMODE_FILL;
    
    /** @brief Culling mode (None, Front, or Back). */
    SDL_GPUCullMode cullMode = SDL_GPU_CULLMODE_NONE;
    
    /** @brief List of pixel formats for each color target. */
    std::vector<SDL_GPUTextureFormat> colorTargetFormats;
    
    /** @brief Whether to enable alpha blending. */
    bool enableBlending = false;

    /** @brief Whether to enable depth testing and writing. */
    bool enableDepthTest = false;

    /** @brief Whether to enable depth writing (requires enableDepthTest). */
    bool enableDepthWrite = true;
    
    /** @brief The comparison operator for the depth test (Less, Greater, etc.). */
    SDL_GPUCompareOp depthCompareOp = SDL_GPU_COMPAREOP_ALWAYS;

    /** @brief Enable GPU-side depth bias (polygon offset) to fight shadow acne. */
    bool  enableDepthBias         = false;
    /** @brief Constant depth offset added to every fragment's depth. */
    float depthBiasConstantFactor = 0.0f;
    /** @brief Slope-proportional depth offset (handles grazing angles). */
    float depthBiasSlopeFactor    = 0.0f;
    /** @brief Maximum absolute depth bias (prevents over-offsetting). */
    float depthBiasClamp          = 0.0f;
};

/**
 * @class GraphicsPipeline
 * @brief Encapsulates a pre-compiled GPU state object.
 * 
 * In modern APIs, all state (shaders, layout, blending) must be 
 * defined upfront in a Pipeline State Object (PSO).
 */
class GraphicsPipeline {
public:
    /**
     * @brief Compiles a new graphics pipeline.
     * @param device Pointer to the active SDL_GPUDevice.
     * @param config The configuration describing the pipeline state.
     * @param renderTargetFormat The pixel format of the target texture (usually the swapchain format).
     */
    GraphicsPipeline(SDL_GPUDevice* device, const PipelineConfig& config, SDL_GPUTextureFormat renderTargetFormat);

    /**
     * @brief Destroys the graphics pipeline and releases GPU resources.
     */
    ~GraphicsPipeline();

    /**
     * @brief Gets the native SDL pipeline handle.
     * @return Pointer to SDL_GPUGraphicsPipeline.
     */
    SDL_GPUGraphicsPipeline* GetHandle() const { return m_Pipeline; }

private:
    SDL_GPUDevice* m_Device;            /**< Pointer to the SDL GPU device. */
    SDL_GPUGraphicsPipeline* m_Pipeline; /**< Native SDL graphics pipeline handle. */
};
