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
    GlobalUniforms m_Globals;
    SDL_Window* m_WindowHandle = nullptr;
    
    uint32_t m_FrameCount = 0;
    float m_TotalTime = 0.0f;
    bool debug = false;
};
