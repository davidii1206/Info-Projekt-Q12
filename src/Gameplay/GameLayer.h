#pragma once
#include "../Core/Layer.h"
#include "World.h"
#include <memory>

// GameLayer wraps the World and feeds it into the layer system.
// Push it onto the LayerStack in Application to activate gameplay.
class GameLayer : public Layer {
public:
    GameLayer() : Layer("GameLayer") {}

    void OnAttach() override {
        m_World = std::make_unique<World>();
    }

    void OnDetach() override {
        m_World.reset();
    }

    void OnUpdate(float dt) override {
        if (m_World)
            m_World->Update(dt);
    }

    void OnEvent(Event& event) override {
        // Forward events to world / game systems here if needed
    }

    World* GetWorld() { return m_World.get(); }

private:
    std::unique_ptr<World> m_World;
};
