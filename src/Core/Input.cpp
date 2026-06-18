/**
 * @file Input.cpp
 * @brief Implementation of the Input utility class.
 */

#include "Input.h"

std::unordered_map<SDL_Keycode, bool> Input::m_Keys;
std::unordered_map<SDL_Keycode, bool> Input::m_KeysLast;
std::unordered_map<uint8_t, bool> Input::m_MouseButtons;
std::unordered_map<uint8_t, bool> Input::m_MouseButtonsLast;
glm::vec2 Input::m_MousePos;
glm::vec2 Input::m_MouseDelta = {0, 0};
float Input::m_MouseWheelDelta = 0.f;

bool Input::m_RelativeMouse = false;

void Input::Update() {
    m_KeysLast = m_Keys;
    m_MouseButtonsLast = m_MouseButtons;
    m_MouseDelta = {0, 0}; // Reset delta each frame
    m_MouseWheelDelta = 0.f; // Reset wheel each frame
    
    float x, y;
    SDL_GetMouseState(&x, &y);
    m_MousePos = {x, y};
}

void Input::ProcessEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_DOWN) {
        m_Keys[event.key.key] = true;
    } else if (event.type == SDL_EVENT_KEY_UP) {
        m_Keys[event.key.key] = false;
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        m_MouseButtons[event.button.button] = true;
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        m_MouseButtons[event.button.button] = false;
    } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
        m_MousePos.x = event.motion.x;
        m_MousePos.y = event.motion.y;
        m_MouseDelta.x += event.motion.xrel;
        m_MouseDelta.y += event.motion.yrel;
    } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        m_MouseWheelDelta += event.wheel.y;
    }
}

bool Input::IsKeyDown(SDL_Keycode key) {
    return m_Keys[key];
}

bool Input::IsKeyPressed(SDL_Keycode key) {
    return m_Keys[key] && !m_KeysLast[key];
}

bool Input::IsKeyReleased(SDL_Keycode key) {
    return !m_Keys[key] && m_KeysLast[key];
}

bool Input::IsMouseButtonDown(uint8_t button) {
    return m_MouseButtons[button];
}

bool Input::IsMouseButtonPressed(uint8_t button) {
    return m_MouseButtons[button] && !m_MouseButtonsLast[button];
}

bool Input::IsMouseButtonReleased(uint8_t button) {
    return !m_MouseButtons[button] && m_MouseButtonsLast[button];
}

glm::vec2 Input::GetMousePosition() {
    return m_MousePos;
}

glm::vec2 Input::GetMouseDelta() {
    return m_MouseDelta;
}

void Input::SetRelativeMouseMode(SDL_Window* window, bool enabled) {
    m_RelativeMouse = enabled;
    SDL_SetWindowRelativeMouseMode(window, enabled);
}

bool Input::IsRelativeMouseMode() {
    return m_RelativeMouse;
}

float Input::GetMouseWheelDelta() {
    return m_MouseWheelDelta;
}
