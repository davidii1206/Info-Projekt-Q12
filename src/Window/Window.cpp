/**
 * @file Window.cpp
 * @brief Implementation of window management and event polling.
 */
#include "Window.h"
#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>
#include <array>

Window* CreateWindow(Window& win) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        spdlog::error("[Window] SDL_Init Error: {}", SDL_GetError());
        return nullptr;
    }

    // Set window flags based on requested mode
    Uint32 flags = SDL_WINDOW_RESIZABLE;
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

    win.hasFocus = true;

    return &win;
}

void DestroyWindow(Window* win) {
    if (!win) return;

    if (win->handle) SDL_DestroyWindow(win->handle);

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