#pragma once
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <glad/glad.h>

#ifdef _WIN32
#undef CreateWindow
#endif

enum class WindowMode { Windowed, Borderless, Fullscreen };

struct Window {
    uint32_t width;
    uint32_t height;
    std::string title;
    WindowMode mode;
    bool vsync;
    SDL_Window* handle = nullptr;
    SDL_GLContext context = nullptr;
    bool hasFocus;
};

Window* CreateWindow(Window& win);
void    DestroyWindow(Window* win);

void    SetWindowMode(Window* win, WindowMode mode);
void    SetResolution(Window* win, int width, int height);
void    SetVsync(Window* win, bool enabled);

std::vector<SDL_Event> PollEvents();

bool    WindowHasFocus(const Window* win);
glm::ivec2 GetWindowSize(const Window* win);

glm::vec2 GetMousePos();
void      SetMousePos(Window* win, float x, float y);
void      HideCursor();
void      ShowCursor();
void      SetRelativeMouseMode(Window* win, bool enabled);