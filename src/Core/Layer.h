#pragma once
#include <string>
#include "Events/Event.h"

class Layer {
public:
    explicit Layer(const std::string& name = "Layer") : m_DebugName(name) {}
    virtual ~Layer() = default;

    // Called when the layer is pushed onto the stack
    virtual void OnAttach() {}
    // Called when the layer is popped from the stack
    virtual void OnDetach() {}
    // Called every frame
    virtual void OnUpdate(float dt) {}
    // Called for every event; set event.Handled = true to consume it
    virtual void OnEvent(Event& event) {}
    // Called every frame for ImGui rendering
    virtual void OnImGuiRender() {}

    const std::string& GetName() const { return m_DebugName; }

protected:
    std::string m_DebugName;
};
