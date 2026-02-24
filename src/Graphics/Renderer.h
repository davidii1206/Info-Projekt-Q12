#pragma once
#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include "Window/Window.h"

class Renderer {
public:
    Renderer(Window* window);
    ~Renderer();

    void BeginFrame(const glm::vec4& clearColor = {0.1f, 0.1f, 0.1f, 1.0f});
    void EndFrame();

private:
    Window* m_Window;
};