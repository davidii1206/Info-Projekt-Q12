/**
 * @file Layer.h
 * @brief Base class for engine layers (update/render/event hooks).
 */

#pragma once
#include <string>
#include "Events/Event.h"

class Renderer;

/**
 * @class Layer
 * @brief A stackable slice of application logic with lifecycle, update,
 *        render and event hooks. Subclassed by e.g. GameLayer, DebugLayer.
 */
class Layer {
public:
    /// @param name Debug name shown in tooling.
    explicit Layer(const std::string& name = "Layer") : m_DebugName(name) {}
    virtual ~Layer() = default; ///< Virtual destructor.

    /// @brief Called when the layer is pushed onto the stack.
    virtual void OnAttach() {}
    /// @brief Called when the layer is popped from the stack.
    virtual void OnDetach() {}
    /// @brief Called every frame. @param dt Delta time in seconds.
    virtual void OnUpdate(float dt) {}
    /// @brief Called for every event; set event.Handled = true to consume it.
    virtual void OnEvent(Event& event) {}

    /// @brief Called every frame for 3D rendering. @param renderer Active renderer.
    virtual void OnRender(Renderer* renderer) {}
    /// @brief Called every frame for ImGui rendering. @param renderer Active renderer.
    virtual void OnImGuiRender(Renderer* renderer) {}

    /// @return The layer's debug name.
    const std::string& GetName() const { return m_DebugName; }

protected:
    std::string m_DebugName; ///< Human-readable layer name.
};
