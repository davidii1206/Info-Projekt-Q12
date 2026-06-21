/**
 * @file KeyEvent.h
 * @brief Keyboard event classes.
 */

#pragma once
#include "Event.h"
#include <SDL3/SDL.h>
#include <sstream>

/**
 * @class KeyEvent
 * @brief Base class for keyboard events; carries the key code.
 */
class KeyEvent : public Event {
public:
    SDL_Keycode GetKeyCode() const { return m_KeyCode; } ///< @return SDL key code.
    EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Keyboard)

protected:
    /// @param keycode The SDL key code.
    explicit KeyEvent(SDL_Keycode keycode) : m_KeyCode(keycode) {}
    SDL_Keycode m_KeyCode; ///< The key code.
};

/**
 * @class KeyPressedEvent
 * @brief Fired when a key is pressed (or auto-repeats).
 */
class KeyPressedEvent : public KeyEvent {
public:
    /// @param keycode The key code. @param repeat Whether this is an auto-repeat.
    explicit KeyPressedEvent(SDL_Keycode keycode, bool repeat = false)
        : KeyEvent(keycode), m_Repeat(repeat) {}

    bool IsRepeat() const { return m_Repeat; } ///< @return Whether this is an auto-repeat.

    /// @return Human-readable description.
    std::string ToString() const override {
        std::ostringstream ss;
        ss << "KeyPressedEvent: " << m_KeyCode << (m_Repeat ? " (repeat)" : "");
        return ss.str();
    }

    EVENT_CLASS_TYPE(KeyPressed)

private:
    bool m_Repeat; ///< Whether the press is an auto-repeat.
};

/**
 * @class KeyReleasedEvent
 * @brief Fired when a key is released.
 */
class KeyReleasedEvent : public KeyEvent {
public:
    /// @param keycode The key code.
    explicit KeyReleasedEvent(SDL_Keycode keycode) : KeyEvent(keycode) {}

    /// @return Human-readable description.
    std::string ToString() const override {
        std::ostringstream ss;
        ss << "KeyReleasedEvent: " << m_KeyCode;
        return ss.str();
    }

    EVENT_CLASS_TYPE(KeyReleased)
};
