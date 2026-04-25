/**
 * @file Scene.h
 * @brief Header for scene management and interface.
 */

#pragma once
#include <entt/entt.hpp>

class NetworkManager;
class Renderer;
class IScene;
class SceneManager;
class PhysicsServer;
class World;

/**
 * @struct SceneContext
 * @brief Context passed to scenes for updates and rendering.
 */
struct SceneContext {
    /// Authoritative registry — only populated when hosting.
    entt::registry& serverRegistry;
    /// Always active registry — used for local rendering and client state.
    entt::registry& clientRegistry;
    /// Reference to the network manager.
    NetworkManager& network;
    /// Reference to the scene manager.
    SceneManager&   scenes;
    /// Pointer to the renderer.
    Renderer*       renderer;
    /// Pointer to the physics server.
    PhysicsServer*  physics;
    /// Pointer to the game world.
    World*          world;
};

/**
 * @class IScene
 * @brief Interface for game scenes.
 */
class IScene {
public:
    virtual ~IScene() = default;

    /**
     * @brief Gets the name of the scene.
     * @return The scene name as a string.
     */
    virtual const char* Name()  const = 0;

    /**
     * @brief Called when the scene is entered.
     * @param ctx The current scene context.
     */
    virtual void OnEnter(SceneContext& ctx) = 0;

    /**
     * @brief Called when the scene is exited.
     * @param ctx The current scene context.
     */
    virtual void OnExit(SceneContext& ctx)  = 0;

    /**
     * @brief Performs per-frame updates (ImGui, input).
     * @param ctx The current scene context.
     * @param dt Delta time for the current frame.
     */
    virtual void FrameUpdate(SceneContext& ctx, float dt) = 0;

    /**
     * @brief Performs fixed-rate updates (logic, networking).
     * @param ctx The current scene context.
     * @param dt Fixed delta time.
     */
    virtual void FixedUpdate(SceneContext& ctx, float dt) = 0;

    /**
     * @brief Renders the scene.
     * @param ctx The current scene context.
     * @param renderer Pointer to the renderer.
     */
    virtual void Render(SceneContext& ctx, Renderer* renderer) = 0;
};

/**
 * @class SceneManager
 * @brief Manages scene transitions and updates.
 */
class SceneManager {
public:
    /**
     * @brief Destructor that cleans up current and queued scenes.
     */
    ~SceneManager();

    /**
     * @brief Queues a scene transition.
     * 
     * Applied at the start of the next World::Update call.
     * Safe to call from inside FrameUpdate or FixedUpdate.
     * 
     * @param next Pointer to the next scene to transition to.
     */
    void RequestTransition(IScene* next);

    /**
     * @brief Flushes any pending scene transitions.
     * 
     * Called by World at the top of Update before any scene updates.
     * @param ctx The current scene context.
     */
    void Flush(SceneContext& ctx);

    /**
     * @brief Updates the current scene's frame logic.
     * @param ctx The current scene context.
     * @param dt Delta time.
     */
    void FrameUpdate(SceneContext& ctx, float dt);

    /**
     * @brief Updates the current scene's fixed logic.
     * @param ctx The current scene context.
     * @param dt Fixed delta time.
     */
    void FixedUpdate(SceneContext& ctx, float dt);

    /**
     * @brief Renders the current scene.
     * @param ctx The current scene context.
     * @param renderer Pointer to the renderer.
     */
    void Render(SceneContext& ctx, Renderer* renderer);

    /**
     * @brief Gets the name of the current scene.
     * @return The name of the current scene.
     */
    const char* GetName() const;

private:
    /// The currently active scene.
    IScene* m_Current = nullptr;
    /// The next scene queued for transition.
    IScene* m_Next    = nullptr;
};
