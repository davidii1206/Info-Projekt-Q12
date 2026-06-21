/**
 * @file MouseEvent.h
 * @brief Mouse movement and button event classes.
 */

#pragma once
#include "Event.h"
#include <SDL3/SDL.h>
#include <sstream>

/**
 * @class MouseMovedEvent
 * @brief Fired when the mouse cursor moves.
 */
class MouseMovedEvent : public Event {
public:
    /// @param x New cursor X. @param y New cursor Y.
    MouseMovedEvent(float x, float y) : m_X(x), m_Y(y) {}

    float GetX() const { return m_X; } ///< @return Cursor X.
    float GetY() const { return m_Y; } ///< @return Cursor Y.

    /// @return Human-readable description.
    std::string ToString() const override {
        std::ostringstream ss;
        ss << "MouseMovedEvent: (" << m_X << ", " << m_Y << ")";
        return ss.str();
    }

    EVENT_CLASS_TYPE(MouseMoved)
    EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Mouse)

private:
    float m_X; ///< Cursor X.
    float m_Y; ///< Cursor Y.
};

/**
 * @class MouseButtonPressedEvent
 * @brief Fired when a mouse button is pressed.
 */
class MouseButtonPressedEvent : public Event {
public:
    /// @param button SDL mouse button index.
    explicit MouseButtonPressedEvent(uint8_t button) : m_Button(button) {}
    uint8_t GetButton() const { return m_Button; } ///< @return SDL mouse button index.

    /// @return Human-readable description.
    std::string ToString() const override {
        std::ostringstream ss;
        ss << "MouseButtonPressedEvent: " << static_cast<int>(m_Button);
        return ss.str();
    }

    EVENT_CLASS_TYPE(MouseButtonPressed)
    EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Mouse)

private:
    uint8_t m_Button; ///< SDL mouse button index.
};

/**
 * @class MouseButtonReleasedEvent
 * @brief Fired when a mouse button is released.
 */
class MouseButtonReleasedEvent : public Event {
public:
    /// @param button SDL mouse button index.
    explicit MouseButtonReleasedEvent(uint8_t button) : m_Button(button) {}
    uint8_t GetButton() const { return m_Button; } ///< @return SDL mouse button index.

    /// @return Human-readable description.
    std::string ToString() const override {
        std::ostringstream ss;
        ss << "MouseButtonReleasedEvent: " << static_cast<int>(m_Button);
        return ss.str();
    }

    EVENT_CLASS_TYPE(MouseButtonReleased)
    EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Mouse)

private:
    uint8_t m_Button; ///< SDL mouse button index.
};
