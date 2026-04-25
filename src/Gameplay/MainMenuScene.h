/**
 * @file MainMenuScene.h
 * @brief Definition of the MainMenuScene class.
 */

#pragma once
#include "Scene.h"
#include "../Graphics/Camera.h"
#include "../Graphics/API/Texture.h"
#include "../Core/WorldManager.h"
#include <memory>
#include <vector>
#include <glm/glm.hpp>

/**
 * @class MainMenuScene
 * @brief Scene representing the main menu of the game.
 * 
 * Handles initial UI, settings, and transitioning to other scenes.
 */
class MainMenuScene : public IScene {
public:
    /**
     * @brief Default constructor for MainMenuScene.
     */
    MainMenuScene();

    /**
     * @brief Gets the name of the scene.
     * @return The string name of the scene.
     */
    const char* Name() const override { return "MainMenuScene"; }

    /**
     * @brief Called when the scene is entered.
     * @param ctx Reference to the SceneContext.
     */
    void OnEnter(SceneContext& ctx) override;

    /**
     * @brief Called when the scene is exited.
     * @param ctx Reference to the SceneContext.
     */
    void OnExit(SceneContext& ctx)  override;

    /**
     * @brief Performs per-frame logic updates (input, camera).
     * @param ctx Reference to the SceneContext.
     * @param dt Delta time since last frame in seconds.
     */
    void LogicUpdate(SceneContext& ctx, float dt) override;

    /**
     * @brief Performs per-frame UI updates (ImGui).
     * @param ctx Reference to the SceneContext.
     * @param dt Delta time since last frame in seconds.
     */
    void UIUpdate(SceneContext& ctx, float dt) override;

    /**
     * @brief Called at a fixed rate for physics and consistent updates.
     * @param ctx Reference to the SceneContext.
     * @param dt Fixed delta time in seconds.
     */
    void FixedUpdate(SceneContext& ctx, float dt) override;

    /**
     * @brief Called to render the scene.
     * @param ctx Reference to the SceneContext.
     * @param renderer Pointer to the renderer instance.
     */
    void Render(SceneContext& ctx, Renderer* renderer) override;

private:
    std::unique_ptr<Camera> m_Camera; /**< Main camera for the menu. */

    // Generation parameters
    WorldGenConfig m_GenConfig;
    Texture* m_DebugTexture = nullptr;
    std::unique_ptr<WorldManager> m_WorldManager;
};
