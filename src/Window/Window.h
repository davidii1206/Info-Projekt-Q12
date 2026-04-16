#pragma once
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

/**
 * @file Window.h
 * @brief Window management and event polling.
 */

#ifdef _WIN32
#undef CreateWindow
#endif

/**
 * @enum WindowMode
 * @brief Window display modes.
 */
enum class WindowMode { Windowed, Borderless, Fullscreen };

/**
 * @struct Window
 * @brief Structure holding SDL window state.
 */
struct Window {
    uint32_t width;  /**< Window width in pixels. */
    uint32_t height; /**< Window height in pixels. */
    std::string title; /**< Window title. */
    WindowMode mode; /**< Window display mode. */
    bool vsync; /**< Vsync enabled status. */
    SDL_Window* handle = nullptr; /**< Pointer to the SDL window handle. */
    bool hasFocus; /**< Whether the window has focus. */
};

/**
 * @brief Creates a system window.
 * @param win Reference to window configuration.
 * @return Pointer to the configured window.
 */
Window* CreateWindow(Window& win);

/**
 * @brief Destroys the system window and cleans up SDL.
 * @param win Pointer to the window structure.
 */
void    DestroyWindow(Window* win);

/**
 * @brief Sets the window display mode.
 * @param win Pointer to the window structure.
 * @param mode The new window mode.
 */
void    SetWindowMode(Window* win, WindowMode mode);

/**
 * @brief Sets the window resolution.
 * @param win Pointer to the window structure.
 * @param width New width.
 * @param height New height.
 */
void    SetResolution(Window* win, int width, int height);

/**
 * @brief Enables or disables Vsync.
 * @param win Pointer to the window structure.
 * @param enabled True to enable, false to disable.
 */
void    SetVsync(Window* win, bool enabled);

/**
 * @brief Polls all pending SDL events.
 * @return A vector of SDL events.
 */
std::vector<SDL_Event> PollEvents();

/**
 * @brief Checks if the window currently has focus.
 * @param win Pointer to the window structure.
 * @return True if focused, false otherwise.
 */
bool    WindowHasFocus(const Window* win);

/**
 * @brief Gets the current window size.
 * @param win Pointer to the window structure.
 * @return glm::ivec2 containing width and height.
 */
glm::ivec2 GetWindowSize(const Window* win);

/**
 * @brief Gets the current mouse position.
 * @return glm::vec2 containing x and y coordinates.
 */
glm::vec2 GetMousePos();

/**
 * @brief Warps the mouse to a specific position in the window.
 * @param win Pointer to the window structure.
 * @param x X coordinate.
 * @param y Y coordinate.
 */
void      SetMousePos(Window* win, float x, float y);

/**
 * @brief Hides the mouse cursor.
 */
void      HideCursor();

/**
 * @brief Shows the mouse cursor.
 */
void      ShowCursor();

/**
 * @brief Sets the relative mouse mode for the window.
 * @param win Pointer to the window structure.
 * @param enabled True to enable relative mode.
 */
void      SetRelativeMouseMode(Window* win, bool enabled);