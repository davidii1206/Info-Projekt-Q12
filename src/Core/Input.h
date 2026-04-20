/**
 * @file Input.h
 * @brief Static utility class for handling keyboard and mouse input.
 */

#pragma once
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <unordered_map>

/**
 * @class Input
 * @brief Static utility class for handling keyboard and mouse input.
 * 
 * Tracks the current and previous states of all keys and mouse buttons 
 * to provide easy access to "pressed" and "released" events.
 */
class Input {
public:
    /**
     * @brief Updates the input state.
     * 
     * Moves current state to the 'last' state to prepare for a new frame.
     * Should be called once per frame.
     */
    static void Update();

    /**
     * @brief Processes an SDL input event.
     * @param event The SDL_Event to parse.
     */
    static void ProcessEvent(const SDL_Event& event);

    /**
     * @brief Checks if a key is currently held down.
     * @param key The SDL keycode to check.
     * @return bool True if the key is down.
     */
    static bool IsKeyDown(SDL_Keycode key);

    /**
     * @brief Checks if a key was pressed exactly this frame.
     * @param key The SDL keycode to check.
     * @return bool True if the key was pressed this frame.
     */
    static bool IsKeyPressed(SDL_Keycode key);

    /**
     * @brief Checks if a key was released exactly this frame.
     * @param key The SDL keycode to check.
     * @return bool True if the key was released this frame.
     */
    static bool IsKeyReleased(SDL_Keycode key);

    /**
     * @brief Checks if a mouse button is currently held down.
     * @param button The SDL mouse button index (e.g., SDL_BUTTON_LEFT).
     * @return bool True if the button is down.
     */
    static bool IsMouseButtonDown(uint8_t button);

    /**
     * @brief Checks if a mouse button was pressed exactly this frame.
     * @param button The SDL mouse button index.
     * @return bool True if the button was pressed this frame.
     */
    static bool IsMouseButtonPressed(uint8_t button);

    /**
     * @brief Checks if a mouse button was released exactly this frame.
     * @param button The SDL mouse button index.
     * @return bool True if the button was released this frame.
     */
    static bool IsMouseButtonReleased(uint8_t button);

    /**
     * @brief Gets the current mouse coordinates in window space.
     * @return glm::vec2 containing x and y position.
     */
    static glm::vec2 GetMousePosition();

    /**
     * @brief Gets the mouse movement delta for the current frame.
     * @return glm::vec2 containing x and y delta.
     */
    static glm::vec2 GetMouseDelta();

    /**
     * @brief Toggles relative mouse mode (hides cursor and captures movement).
     * @param window The SDL window to capture.
     * @param enabled If true, mouse is captured.
     */
    static void SetRelativeMouseMode(SDL_Window* window, bool enabled);

    /** @brief Returns true if relative mouse mode is active. */
    static bool IsRelativeMouseMode();

private:
    static std::unordered_map<SDL_Keycode, bool> m_Keys; ///< Current key state map.
    static std::unordered_map<SDL_Keycode, bool> m_KeysLast; ///< Key state map of the previous frame.
    
    static std::unordered_map<uint8_t, bool> m_MouseButtons; ///< Current mouse button state map.
    static std::unordered_map<uint8_t, bool> m_MouseButtonsLast; ///< Mouse button state map of the previous frame.

    static glm::vec2 m_MousePos; ///< Current mouse position in window space.
    static glm::vec2 m_MouseDelta; ///< Mouse movement delta for the current frame.
    static bool m_RelativeMouse; ///< Flag for relative mouse mode activation.
};
