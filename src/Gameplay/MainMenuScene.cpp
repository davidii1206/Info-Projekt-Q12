#include "MainMenuScene.h"
#include "LobbyScene.h"
#include "../Networking/NetworkManager.h"
#include "../Core/Input.h"
#include "../Graphics/Renderer.h"
#include "../Core/Biome.h"
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
    if (ctx.network.IsHosting()) {
        ctx.scenes.RequestTransition(new LobbyScene());
    }
    m_Camera->Update(dt);
}

/**
 * @brief Called every frame to update scene UI.
 */
void MainMenuScene::UIUpdate(SceneContext& ctx, float /*dt*/) {
    ImGui::Begin("Main Menu");
    ImGui::Text("Bugmin Engine - Main Menu");
    if (ImGui::Button("Host Game")) {
        if (ctx.network.StartHost()) {
            spdlog::info("Hosting game on port 25565");
        }
    }
    ImGui::InputText("IP", m_ConnectIP, sizeof(m_ConnectIP));
    if (ImGui::Button("Connect") && !ctx.network.IsConnected()) {
        if (ctx.network.Connect(m_ConnectIP)) {
            spdlog::info("Connecting to {}", m_ConnectIP);
        }
    }
    if (ctx.network.IsConnected()) {
        ImGui::SameLine();
        if (ImGui::Button("Open Lobby")) {
            ctx.scenes.RequestTransition(new LobbyScene());
        }
    }
    if (ImGui::Button("Quit")) {
        SDL_Event quitEvent;
        quitEvent.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quitEvent);
    }
    ImGui::End();

    // Terrain Generation Debug Window
    ImGui::Begin("World Generation (WorldManager)");
    ImGui::SliderInt("Terrains", &m_GenConfig.numTerrains, 4, 64);
    ImGui::SliderInt("Relaxation", &m_GenConfig.relaxationIterations, 0, 10);

    ImGui::SeparatorText("Terraced Terrain");
    ImGui::SliderInt("Tiers", &m_GenConfig.numTiers, 1, 12);
    ImGui::SliderFloat("Tier Height", &m_GenConfig.tierHeight, 0.5f, 10.0f);
    ImGui::SliderInt("Water Tier", &m_GenConfig.waterTier, 0, 4);
    ImGui::DragFloat("Tier Noise Scale", &m_GenConfig.tierNoiseScale, 0.001f, 0.001f, 0.5f);
    ImGui::DragFloat("World Extent", &m_GenConfig.worldExtent, 1.0f, 50.0f, 1000.0f);
    ImGui::DragFloat("Tile Size", &m_GenConfig.tileSize, 0.1f, 0.25f, 8.0f);
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
        if (ImGui::BeginTable("SpawnPointsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("ID");
            ImGui::TableSetupColumn("Bug Class");
            ImGui::TableSetupColumn("Position");
            ImGui::TableHeadersRow();

            for (const auto& td : m_WorldManager->GetTerrains()) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", td.id);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", BugClassToString(td.bugClass));

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("(%.0f, %.0f)", td.site.x, td.site.y);
            }
            ImGui::EndTable();
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
