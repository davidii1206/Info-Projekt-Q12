/**
 * @file MainMenuScene.cpp
 * @brief Implementation of the MainMenuScene class.
 */

#include "MainMenuScene.h"
#include "GameScene.h"
#include "../Networking/NetworkManager.h"
#include "../Core/Input.h"
#include "../Graphics/Renderer.h"
#include <imgui.h>

/**
 * @brief Default constructor for MainMenuScene.
 * 
 * Initializes the camera with default values.
 */
MainMenuScene::MainMenuScene() {
    m_Camera = std::make_unique<Camera>();
}

/**
 * @brief Called when the scene is entered.
 * 
 * Clears current server and client registries.
 * @param ctx Reference to the SceneContext.
 */
void MainMenuScene::OnEnter(SceneContext& ctx) {
    ctx.serverRegistry.clear();
    ctx.clientRegistry.clear();
}

/**
 * @brief Called when the scene is exited.
 * @param ctx Reference to the SceneContext.
 */
void MainMenuScene::OnExit(SceneContext& ctx) {}

/**
 * @brief Called every frame to update scene logic.
 * 
 * Checks for network connection to trigger transitions and handles free-fly camera movement.
 * @param ctx Reference to the SceneContext.
 * @param dt Delta time since last frame in seconds.
 */
void MainMenuScene::FrameUpdate(SceneContext& ctx, float dt) {
    /** 
     * Transition automatically when the network becomes active.
     * The Host/Join action itself comes from NetworkDebugUI.
     */
    if (ctx.network.IsConnected()) {
        ctx.scenes.RequestTransition(new GameScene());
    }

    /** Camera movement even in menu for testing input. */
    if (Input::IsRelativeMouseMode()) {
        glm::vec2 delta = Input::GetMouseDelta();
        m_Camera->Rotate(delta.x, delta.y);

        if (Input::IsKeyDown(SDLK_W)) m_Camera->MoveForward(dt);
        if (Input::IsKeyDown(SDLK_S)) m_Camera->MoveBackward(dt);
        if (Input::IsKeyDown(SDLK_A)) m_Camera->MoveLeft(dt);
        if (Input::IsKeyDown(SDLK_D)) m_Camera->MoveRight(dt);
        if (Input::IsKeyDown(SDLK_SPACE)) m_Camera->MoveUp(dt);
        if (Input::IsKeyDown(SDLK_LSHIFT)) m_Camera->MoveDown(dt);
    }
    m_Camera->Update(dt);

    ImGui::Begin("Main Menu");
    ImGui::Text("Bugmin Engine - Main Menu");
    ImGui::Text("F1 to toggle Free-Fly Camera");
    if (ImGui::Button("Quit")) {
        SDL_Event quitEvent;
        quitEvent.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quitEvent);
    }
    ImGui::End();

    if (Input::IsKeyPressed(SDLK_F1)) {
        bool newState = !Input::IsRelativeMouseMode();
        Input::SetRelativeMouseMode(ctx.renderer->GetWindow()->handle, newState);
    }
}

/**
 * @brief Called at a fixed rate for physics and consistent updates.
 * @param ctx Reference to the SceneContext.
 * @param dt Fixed delta time in seconds.
 */
void MainMenuScene::FixedUpdate(SceneContext& ctx, float dt) {}

/**
 * @brief Called to render the scene.
 * 
 * Updates Global Uniforms so shaders have a valid camera even in menu.
 * @param ctx Reference to the SceneContext.
 * @param renderer Pointer to the renderer instance.
 */
void MainMenuScene::Render(SceneContext& ctx, Renderer* renderer) {
    int w, h;
    SDL_GetWindowSizeInPixels(renderer->GetWindow()->handle, &w, &h);
    float aspect = (float)w / (float)h;

    GlobalUniforms& globals = renderer->GetGlobalUniforms();
    globals.view = m_Camera->GetViewMatrix();
    globals.proj = m_Camera->GetProjectionMatrix(aspect);
    globals.viewProj = globals.proj * globals.view;
    globals.cameraPos = glm::vec4(m_Camera->m_Position, 1.0f);
    
    renderer->UpdateGlobalUniforms(globals);
}
