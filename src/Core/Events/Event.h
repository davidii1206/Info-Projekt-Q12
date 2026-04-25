#pragma once
#include <string>
#include <functional>

enum class EventType {
    None = 0,
    KeyPressed, KeyReleased,
    MouseMoved, MouseButtonPressed, MouseButtonReleased,
    WindowResize, WindowClose,
    EntityDamaged, EntityDied, BiomeChanged, Footstep, UIClick, MusicRequest
};

enum class EventCategory {
    None        = 0,
    Input       = 1 << 0,
    Keyboard    = 1 << 1,
    Mouse       = 1 << 2,
    Window      = 1 << 3,
    Sound       = 1 << 4,
    Game        = 1 << 5
};

inline int operator|(EventCategory a, EventCategory b) {
    return static_cast<int>(a) | static_cast<int>(b);
}

class Event {
public:
    virtual ~Event() = default;

    virtual EventType       GetType()       const = 0;
    virtual const char*     GetName()       const = 0;
    virtual int             GetCategories() const = 0;
    virtual std::string     ToString()      const { return GetName(); }

    bool IsInCategory(EventCategory category) const {
        return GetCategories() & static_cast<int>(category);
    }

    bool Handled = false;
};

// Macro to reduce boilerplate in concrete event classes
#define EVENT_CLASS_TYPE(type)                                          \
    static  EventType GetStaticType() { return EventType::type; }      \
    virtual EventType GetType()  const override { return GetStaticType(); } \
    virtual const char* GetName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(cat)                                       \
    virtual int GetCategories() const override { return static_cast<int>(cat); }


// ── Dispatcher ──────────────────────────────────────────────────────────────

class EventDispatcher {
public:
    explicit EventDispatcher(Event& event) : m_Event(event) {}

    // Calls fn(event) if the event type matches T; marks it handled on true return.
    template<typename T, typename Fn>
    bool Dispatch(const Fn& fn) {
        if (m_Event.GetType() == T::GetStaticType()) {
            m_Event.Handled |= fn(static_cast<T&>(m_Event));
            return true;
        }
        return false;
    }

private:
    Event& m_Event;
};
