/**
 * @file WindowEvent.h
 * @brief Window resize and close event classes.
 */

#pragma once
#include "Event.h"
#include <sstream>

/**
 * @class WindowResizeEvent
 * @brief Fired when the window is resized.
 */
class WindowResizeEvent : public Event {
public:
    /// @param width New width in pixels. @param height New height in pixels.
    WindowResizeEvent(int width, int height) : m_Width(width), m_Height(height) {}

    int GetWidth()  const { return m_Width; }  ///< @return New width in pixels.
    int GetHeight() const { return m_Height; } ///< @return New height in pixels.

    /// @return Human-readable description.
    std::string ToString() const override {
        std::ostringstream ss;
        ss << "WindowResizeEvent: " << m_Width << "x" << m_Height;
        return ss.str();
    }

    EVENT_CLASS_TYPE(WindowResize)
    EVENT_CLASS_CATEGORY(EventCategory::Window)

private:
    int m_Width;  ///< New width in pixels.
    int m_Height; ///< New height in pixels.
};

/**
 * @class WindowCloseEvent
 * @brief Fired when the window is requested to close.
 */
class WindowCloseEvent : public Event {
public:
    WindowCloseEvent() = default; ///< Default constructor.

    EVENT_CLASS_TYPE(WindowClose)
    EVENT_CLASS_CATEGORY(EventCategory::Window)
};
