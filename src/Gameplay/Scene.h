#pragma once
#include <entt/entt.hpp>

class NetworkManager;
class Renderer;
class IScene;
class SceneManager; // forward-declared so SceneContext can reference it

struct SceneContext {
    entt::registry& serverRegistry; // authoritative — only populated when hosting
    entt::registry& clientRegistry; // always active — teammate reads this for rendering
    NetworkManager& network;
    SceneManager&   scenes;
};

class IScene {
public:
    virtual ~IScene() = default;
    virtual const char* Name()  const = 0;
    virtual void OnEnter(SceneContext& ctx) = 0;
    virtual void OnExit(SceneContext& ctx)  = 0;
    virtual void FrameUpdate(SceneContext& ctx, float dt) = 0; // per-frame: ImGui, input
    virtual void FixedUpdate(SceneContext& ctx, float dt) = 0; // fixed-rate: logic, networking
    virtual void Render(SceneContext& ctx, Renderer* renderer) = 0;
};

class SceneManager {
public:
    ~SceneManager();

    // Queue a scene transition; applied at the start of the next World::Update call.
    // Safe to call from inside FrameUpdate or FixedUpdate.
    void RequestTransition(IScene* next);

    // Called by World at the top of Update before any scene updates.
    void Flush(SceneContext& ctx);

    void FrameUpdate(SceneContext& ctx, float dt);
    void FixedUpdate(SceneContext& ctx, float dt);
    void Render(SceneContext& ctx, Renderer* renderer);

    const char* GetName() const;

private:
    IScene* m_Current = nullptr;
    IScene* m_Next    = nullptr;
};
