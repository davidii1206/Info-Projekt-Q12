/**
 * @file Event.h
 * @brief Base event type, categories, the dispatcher and helper macros for the
 *        engine's event system.
 */

#pragma once
#include <string>
#include <functional>

/**
 * @enum EventType
 * @brief Concrete type tag for every event class.
 */
enum class EventType {
    None = 0,                                            ///< No/unknown event.
    KeyPressed, KeyReleased,                             ///< Keyboard events.
    MouseMoved, MouseButtonPressed, MouseButtonReleased, ///< Mouse events.
    WindowResize, WindowClose,                           ///< Window events.
    EntityDamaged, EntityDied, BiomeChanged, Footstep, UIClick, MusicRequest ///< Game/sound events.
};

/**
 * @enum EventCategory
 * @brief Bit flags grouping events so handlers can filter by category.
 */
enum class EventCategory {
    None        = 0,      ///< No category.
    Input       = 1 << 0, ///< Any input event.
    Keyboard    = 1 << 1, ///< Keyboard input.
    Mouse       = 1 << 2, ///< Mouse input.
    Window      = 1 << 3, ///< Window/system event.
    Sound       = 1 << 4, ///< Drives audio.
    Game        = 1 << 5  ///< Gameplay event.
};

/// @brief Bitwise-OR of two categories, yielding the combined flag mask.
inline int operator|(EventCategory a, EventCategory b) {
    return static_cast<int>(a) | static_cast<int>(b);
}

/**
 * @class Event
 * @brief Abstract base class for all events.
 */
class Event {
public:
    virtual ~Event() = default; ///< Virtual destructor.

    virtual EventType       GetType()       const = 0; ///< @return The event's concrete type.
    virtual const char*     GetName()       const = 0; ///< @return The event's name.
    virtual int             GetCategories() const = 0; ///< @return Bitmask of EventCategory flags.
    virtual std::string     ToString()      const { return GetName(); } ///< @return Human-readable description.

    /// @brief Tests whether the event belongs to a category.
    bool IsInCategory(EventCategory category) const {
        return GetCategories() & static_cast<int>(category);
    }

    bool Handled = false; ///< Set true once a handler has consumed the event.
};

/// @def EVENT_CLASS_TYPE
/// @brief Implements the type-identification members for a concrete event.
#define EVENT_CLASS_TYPE(type)                                          \
    static  EventType GetStaticType() { return EventType::type; }      \
    virtual EventType GetType()  const override { return GetStaticType(); } \
    virtual const char* GetName() const override { return #type; }

/// @def EVENT_CLASS_CATEGORY
/// @brief Implements GetCategories() for a concrete event.
#define EVENT_CLASS_CATEGORY(cat)                                       \
    virtual int GetCategories() const override { return static_cast<int>(cat); }


// ── Dispatcher ──────────────────────────────────────────────────────────────

/**
 * @class EventDispatcher
 * @brief Routes an event to a typed handler if the runtime type matches.
 */
class EventDispatcher {
public:
    /// @param event The event to dispatch.
    explicit EventDispatcher(Event& event) : m_Event(event) {}

    /**
     * @brief Calls @p fn(event) if the event's type matches T.
     * @tparam T  Expected event type.
     * @tparam Fn Callable taking a T& and returning bool (true = handled).
     * @return True if the type matched (and @p fn was invoked).
     */
    template<typename T, typename Fn>
    bool Dispatch(const Fn& fn) {
        if (m_Event.GetType() == T::GetStaticType()) {
            m_Event.Handled |= fn(static_cast<T&>(m_Event));
            return true;
        }
        return false;
    }

private:
    Event& m_Event; ///< The event being dispatched.
};
