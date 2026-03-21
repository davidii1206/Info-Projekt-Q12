#pragma once
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include "Graphics/Renderer.h"
#include "Graphics/Model.h"
#include "Graphics/Camera.h"
#include "Graphics/GlobalUniforms.h"
#include "Core/AssetManager.h"

/**
 * @class TestScene
 * @brief Standalone testing scene for rendering GLTF models.
 */
class TestScene {
public:
    TestScene(Renderer* renderer);
    ~TestScene();

    void Update(float deltaTime);
    void Render(Renderer* renderer);
    void OnImGui();

private:
    void InitPipelines(Renderer* renderer);

    SceneData m_Scene;
    std::unique_ptr<Camera> m_Camera;
    
    // Shaders
    std::unique_ptr<Shader> m_VertShader;
    std::unique_ptr<Shader> m_FragShader;
    
    GraphicsPipeline* m_Pipeline = nullptr;
    GraphicsPipeline* m_GBufferPipeline = nullptr;
    GraphicsPipeline* m_PostPipeline = nullptr;
    GlobalUniforms m_Globals;
    SDL_Window* m_WindowHandle = nullptr;
    
    // G-Buffer
    std::unique_ptr<Framebuffer> m_GBuffer;
    std::vector<SDL_GPUTextureFormat> m_GBufferFormats;

    // Shaders
    std::unique_ptr<Shader> m_GBufferVertShader;
    std::unique_ptr<Shader> m_GBufferFragShader;
    std::unique_ptr<Shader> m_PostVertShader;
    std::unique_ptr<Shader> m_PostFragShader;
    
    // Post Processing Params
    float m_DepthThreshold = 0.375f;
    float m_NormalThreshold = 0.375f;
    float m_PosterizeSteps = 5.0f;
    float m_DarkenFactor = 0.5f;
    float m_LightenFactor = 0.3f;
    int m_DownscaleFactor = 4;
    int m_DebugMode = 0;
    
    uint32_t m_FrameCount = 0;
    float m_TotalTime = 0.0f;
    bool debug = false;
};
