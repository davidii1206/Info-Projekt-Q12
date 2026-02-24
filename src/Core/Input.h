#pragma once
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <unordered_map>

class Input {
public:
    static void Update();
    static void ProcessEvent(const SDL_Event& event);

    static bool IsKeyDown(SDL_Keycode key);
    static bool IsKeyPressed(SDL_Keycode key);
    static bool IsKeyReleased(SDL_Keycode key);

    static bool IsMouseButtonDown(uint8_t button);
    static bool IsMouseButtonPressed(uint8_t button);
    static bool IsMouseButtonReleased(uint8_t button);

    static glm::vec2 GetMousePosition();
    static glm::vec2 GetMouseDelta();

private:
    static std::unordered_map<SDL_Keycode, bool> m_Keys;
    static std::unordered_map<SDL_Keycode, bool> m_KeysLast;
    
    static std::unordered_map<uint8_t, bool> m_MouseButtons;
    static std::unordered_map<uint8_t, bool> m_MouseButtonsLast;

    static glm::vec2 m_MousePos;
    static glm::vec2 m_MouseLastPos;
};