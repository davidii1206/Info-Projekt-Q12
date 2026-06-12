#pragma once
#include "Event.h"
#include <SDL3/SDL.h>
#include <sstream>

class KeyEvent : public Event {
public:
    SDL_Keycode GetKeyCode() const { return m_KeyCode; }
    EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Keyboard)

protected:
    explicit KeyEvent(SDL_Keycode keycode) : m_KeyCode(keycode) {}
    SDL_Keycode m_KeyCode;
};

class KeyPressedEvent : public KeyEvent {
public:
    explicit KeyPressedEvent(SDL_Keycode keycode, bool repeat = false)
        : KeyEvent(keycode), m_Repeat(repeat) {}

    bool IsRepeat() const { return m_Repeat; }

    std::string ToString() const override {
        std::ostringstream ss;
        ss << "KeyPressedEvent: " << m_KeyCode << (m_Repeat ? " (repeat)" : "");
        return ss.str();
    }

    EVENT_CLASS_TYPE(KeyPressed)

private:
    bool m_Repeat;
};

class KeyReleasedEvent : public KeyEvent {
public:
    explicit KeyReleasedEvent(SDL_Keycode keycode) : KeyEvent(keycode) {}

    std::string ToString() const override {
        std::ostringstream ss;
        ss << "KeyReleasedEvent: " << m_KeyCode;
        return ss.str();
    }

    EVENT_CLASS_TYPE(KeyReleased)
};
