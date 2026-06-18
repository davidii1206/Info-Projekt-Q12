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
    m_WorldManager = std::make_unique<WorldManager>();
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

    m_WorldManager->Generate(m_GenConfig);
    m_WorldManager->UpdateDebugTexture(ctx.renderer->GetDevice(), &m_DebugTexture);
}

/**
 * @brief Called when the scene is exited.
 * @param ctx Reference to the SceneContext.
 */
void MainMenuScene::OnExit(SceneContext& ctx) {
    if (m_DebugTexture) {
        delete m_DebugTexture;
        m_DebugTexture = nullptr;
    }
}

/**
 * @brief Called every frame to update scene logic.
 */
void MainMenuScene::LogicUpdate(SceneContext& ctx, float dt) {
    if (ctx.network.IsConnected()) {
        ctx.scenes.RequestTransition(new GameScene());
    }

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

    if (Input::IsKeyPressed(SDLK_F1)) {
        bool newState = !Input::IsRelativeMouseMode();
        Input::SetRelativeMouseMode(ctx.renderer->GetWindow()->handle, newState);
    }
}

/**
 * @brief Called every frame to update scene UI.
 */
void MainMenuScene::UIUpdate(SceneContext& ctx, float /*dt*/) {
    ImGui::Begin("Main Menu");
    ImGui::Text("Bugmin Engine - Main Menu");
    ImGui::Text("F1 to toggle Free-Fly Camera");
    if (ImGui::Button("Host Game")) {
        if (ctx.network.StartHost()) {
            spdlog::info("Hosting game on port 25565");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Quit")) {
        SDL_Event quitEvent;
        quitEvent.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quitEvent);
    }
    ImGui::End();

    // Terrain Generation Debug Window
    ImGui::Begin("World Generation (WorldManager)");
    ImGui::SliderInt("Terrains", &m_GenConfig.numTerrains, 2, 16);
    ImGui::SliderInt("Relaxation", &m_GenConfig.relaxationIterations, 0, 10);
    ImGui::Checkbox("Random Spawn In Territory", &m_GenConfig.randomSpawnInTerrain);
    
    ImGui::SeparatorText("Thronefall Style");
    ImGui::SliderInt("Altitude Levels", &m_GenConfig.numAltitudeLevels, 1, 8);
    ImGui::DragFloat("Altitude Noise Scale", &m_GenConfig.altitudeNoiseScale, 0.001f, 0.0001f, 0.1f);
    ImGui::SliderFloat("Altitude Max Height", &m_GenConfig.altitudeMaxHeight, 0.1f, 1.0f);
    ImGui::SliderFloat("Slope Sharpness", &m_GenConfig.slopeSharpness, 0.01f, 0.49f);
    ImGui::SliderFloat("Slope Width", &m_GenConfig.slopeWidth, 1.0f, 100.0f);
    ImGui::Checkbox("Show Heightmap", &m_GenConfig.showHeightmap);

    ImGui::SeparatorText("Noise Parameters");
    ImGui::DragFloat("Noise Scale", &m_GenConfig.noiseScale, 0.001f, 0.0001f, 1.0f);
    ImGui::DragInt("Noise Octaves", &m_GenConfig.noiseOctaves, 1, 1, 8);
    ImGui::DragInt("Seed", &m_GenConfig.seed, 1, 0, 999999);
    
    if (ImGui::Button("Generate")) {
        m_WorldManager->Generate(m_GenConfig);
        m_WorldManager->UpdateDebugTexture(ctx.renderer->GetDevice(), &m_DebugTexture);
    }

    if (m_DebugTexture) {
        ImTextureID texID = (ImTextureID)m_DebugTexture->GetHandle();
        ImGui::Image(texID, ImVec2((float)m_GenConfig.width, (float)m_GenConfig.height));
    }

    if (ImGui::CollapsingHeader("Spawn Points")) {
        for (const auto& td : m_WorldManager->GetTerrains()) {
            ImGui::Text("Base %d: (%.1f, %.1f)", td.id, td.spawnPoint.x, td.spawnPoint.y);
        }
    }
    ImGui::End();
}

/**
 * @brief Called at a fixed rate for physics and consistent updates.
 */
void MainMenuScene::FixedUpdate(SceneContext& /*ctx*/, float /*dt*/) {}

/**
 * @brief Called to render the scene.
 */
void MainMenuScene::Render(SceneContext& /*ctx*/, Renderer* renderer) {
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
