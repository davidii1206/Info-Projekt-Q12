#pragma once
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include "Window/Window.h"

struct SDL_GPUDevice;
struct SDL_GPUCommandBuffer;
struct SDL_GPURenderPass;
struct SDL_GPUTexture;

class Renderer {
public:
    Renderer(Window* window);
    ~Renderer();

    void BeginFrame(const glm::vec4& clearColor = {0.1f, 0.1f, 0.1f, 1.0f});
    void EndFrame();

private:
    Window* m_Window;
    SDL_GPUDevice* m_Device;
    SDL_GPUCommandBuffer* m_CurrentCommandBuffer;
    SDL_GPURenderPass* m_CurrentRenderPass;
    SDL_GPUTexture* m_CurrentSwapchainTexture;
};