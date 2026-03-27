#pragma once
#include "Event.h"
#include <SDL3/SDL.h>
#include <sstream>

class MouseMovedEvent : public Event {
public:
    MouseMovedEvent(float x, float y) : m_X(x), m_Y(y) {}

    float GetX() const { return m_X; }
    float GetY() const { return m_Y; }

    std::string ToString() const override {
        std::ostringstream ss;
        ss << "MouseMovedEvent: (" << m_X << ", " << m_Y << ")";
        return ss.str();
    }

    EVENT_CLASS_TYPE(MouseMoved)
    EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Mouse)

private:
    float m_X, m_Y;
};

class MouseButtonPressedEvent : public Event {
public:
    explicit MouseButtonPressedEvent(uint8_t button) : m_Button(button) {}
    uint8_t GetButton() const { return m_Button; }

    std::string ToString() const override {
        std::ostringstream ss;
        ss << "MouseButtonPressedEvent: " << static_cast<int>(m_Button);
        return ss.str();
    }

    EVENT_CLASS_TYPE(MouseButtonPressed)
    EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Mouse)

private:
    uint8_t m_Button;
};

class MouseButtonReleasedEvent : public Event {
public:
    explicit MouseButtonReleasedEvent(uint8_t button) : m_Button(button) {}
    uint8_t GetButton() const { return m_Button; }

    std::string ToString() const override {
        std::ostringstream ss;
        ss << "MouseButtonReleasedEvent: " << static_cast<int>(m_Button);
        return ss.str();
    }

    EVENT_CLASS_TYPE(MouseButtonReleased)
    EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Mouse)

private:
    uint8_t m_Button;
};
