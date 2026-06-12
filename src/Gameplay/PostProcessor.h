/**
 * @file PostProcessor.h
 * @brief Manages the post-processing pipeline and G-Buffer.
 */

#pragma once
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "Graphics/Renderer.h"
#include "Graphics/API/Shader.h"
#include "Graphics/API/GraphicsPipeline.h"
#include "Graphics/API/Framebuffer.h"

/**
 * @class PostProcessor
 * @brief Manages the G-Buffer lifecycle and post-processing effects.
 * 
 * Handles creation and resizing of the G-Buffer, initialization of 
 * post-processing shaders and pipelines, and execution of the final
 * post-processing pass.
 */
class PostProcessor {
public:
    /**
     * @brief Constructs a PostProcessor and initializes its resources.
     * @param renderer Pointer to the renderer instance.
     */
    PostProcessor(Renderer* renderer);

    /**
     * @brief Destroys the PostProcessor and releases resources.
     */
    ~PostProcessor();

    /**
     * @brief Updates post-processing logic.
     * @param deltaTime Time elapsed since the last frame.
     */
    void Update(float deltaTime);
    
    /**
     * @brief Sets up the G-Buffer and updates screen-related global uniforms.
     * @param renderer Pointer to the renderer instance.
     */
    void BeginFrame(Renderer* renderer);
    
    /**
     * @brief Performs the final post-processing pass to the swapchain.
     * @param renderer Pointer to the renderer instance.
     */
    void EndFrame(Renderer* renderer);
    
    /**
     * @brief Renders ImGui debug UI for post-processing settings.
     */
    void OnImGui();

private:
    /**
     * @brief Initializes the graphics pipelines for post-processing.
     * @param renderer Pointer to the renderer instance.
     */
    void InitPipelines(Renderer* renderer);

    /// The G-Buffer framebuffer.
    std::unique_ptr<Framebuffer> m_GBuffer;
    /// Formats for each G-Buffer target.
    std::vector<SDL_GPUTextureFormat> m_GBufferFormats;

    /// Post-processing vertex shader.
    std::unique_ptr<Shader> m_PostVertShader;
    /// Post-processing fragment shader.
    std::unique_ptr<Shader> m_PostFragShader;
    /// Post-processing graphics pipeline.
    GraphicsPipeline* m_PostPipeline = nullptr;
    
    /// Threshold for depth-based edge detection.
    float m_DepthThreshold = 0.375f;
    /// Threshold for normal-based edge detection.
    float m_NormalThreshold = 0.375f;
    /// Number of steps for posterization effect.
    float m_PosterizeSteps = 5.0f;
    /// Factor by which to downscale the G-Buffer.
    int m_DownscaleFactor = 3;
    /// Current debug visualization mode.
    int m_DebugMode = 0;
};
