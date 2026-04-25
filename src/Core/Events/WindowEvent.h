#pragma once
#include "Event.h"
#include <sstream>

class WindowResizeEvent : public Event {
public:
    WindowResizeEvent(int width, int height) : m_Width(width), m_Height(height) {}

    int GetWidth()  const { return m_Width; }
    int GetHeight() const { return m_Height; }

    std::string ToString() const override {
        std::ostringstream ss;
        ss << "WindowResizeEvent: " << m_Width << "x" << m_Height;
        return ss.str();
    }

    EVENT_CLASS_TYPE(WindowResize)
    EVENT_CLASS_CATEGORY(EventCategory::Window)

private:
    int m_Width, m_Height;
};

class WindowCloseEvent : public Event {
public:
    WindowCloseEvent() = default;

    EVENT_CLASS_TYPE(WindowClose)
    EVENT_CLASS_CATEGORY(EventCategory::Window)
};
