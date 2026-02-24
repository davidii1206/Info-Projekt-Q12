#include "Window.h"
#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>
#include <array>
#include <glad/glad.h>

Window* CreateWindow(Window& win) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        spdlog::error("[Window] SDL_Init Error: {}", SDL_GetError());
        return nullptr;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (win.mode == WindowMode::Fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    } 
    else if (win.mode == WindowMode::Borderless) {
        flags |= SDL_WINDOW_BORDERLESS;
    }

    win.handle = SDL_CreateWindow(
        win.title.c_str(),
        win.width,
        win.height,
        flags
    );

    if (!win.handle) {
        spdlog::error("[Window] SDL_CreateWindow Error: {}", SDL_GetError());
        return nullptr;
    }

    win.context = SDL_GL_CreateContext(win.handle);
    if (!win.context) {
        spdlog::error("[Window] SDL_GL_CreateContext Error: {}", SDL_GetError());
        SDL_DestroyWindow(win.handle);
        win.handle = nullptr;
        return nullptr;
    }

    SDL_GL_SetSwapInterval(win.vsync ? 1 : 0);
    win.hasFocus = true;

    return &win;
}

void DestroyWindow(Window* win) {
    if (!win) return;

    if (win->context) SDL_GL_DestroyContext(win->context);
    if (win->handle) SDL_DestroyWindow(win->handle);

    win->context = nullptr;
    win->handle = nullptr;

    SDL_Quit();
}

void SetWindowMode(Window* win, WindowMode mode) {
    if (!win) return;

    Uint32 flags = 0;
    switch (mode) {
        case WindowMode::Windowed:    flags = 0; break;
        case WindowMode::Borderless:  flags = SDL_WINDOW_BORDERLESS; break;
        case WindowMode::Fullscreen:  flags = SDL_WINDOW_FULLSCREEN; break;
    }

    SDL_SetWindowFullscreen(win->handle, flags);
    win->mode = mode;
}

void SetResolution(Window* win, int width, int height) {
    if (!win || !win->handle) return;

    SDL_SetWindowSize(win->handle, width, height);
    win->width = width;
    win->height = height;
}

void SetVsync(Window* win, bool enabled) {
    if (!win || !win->context) return;
    SDL_GL_SetSwapInterval(enabled ? 1 : 0);
    win->vsync = enabled;
}

std::vector<SDL_Event> PollEvents() {
    std::vector<SDL_Event> events;
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        events.push_back(event);
    }
    return events;
}

bool WindowHasFocus(const Window* win) {
    if (!win) return false;
    return win->hasFocus;
}

glm::ivec2 GetWindowSize(const Window* win) {
    if (!win) return glm::ivec2(0);
    return glm::ivec2(win->width, win->height);
}

glm::vec2 GetMousePos() {
    float x, y;
    SDL_GetMouseState(&x, &y);
    return glm::vec2{x, y};
}

void SetMousePos(Window* win, float x, float y) {
    SDL_WarpMouseInWindow(win->handle, x, y);
}

void HideCursor() {
    SDL_HideCursor();
}

void ShowCursor() {
    SDL_ShowCursor();
}

void SetRelativeMouseMode(Window* win, bool enabled) {
    SDL_SetWindowRelativeMouseMode(win->handle, enabled ? true : false);
}