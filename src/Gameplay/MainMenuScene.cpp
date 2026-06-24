#include "MainMenuScene.h"
#include "LobbyScene.h"
#include "../Networking/NetworkManager.h"
#include "../Core/Input.h"
#include "../Core/DebugUI.h"
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
    if (Input::IsKeyPressed(SDLK_F12)) DebugUI::Toggle();

    if (ctx.network.IsHosting()) {
        ctx.scenes.RequestTransition(new LobbyScene());
    }
    m_Camera->Update(dt);
}

/**
 * @brief Called every frame to update scene UI.
 */
void MainMenuScene::UIUpdate(SceneContext& ctx, float /*dt*/) {
    // Center the main menu in the middle of the screen, fixed width.
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(380.f, 0.f), ImGuiCond_Always);
    }
    ImGui::Begin("Bugmin", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::TextColored(ImVec4(0.85f, 0.70f, 0.30f, 1.f), "Bugmin");
    ImGui::TextDisabled("Insekten-RTS - Hauptmenue");
    ImGui::Separator();

    const float W = ImGui::GetContentRegionAvail().x;
    if (ImGui::Button("Host Game", ImVec2(W, 32))) {
        if (ctx.network.StartHost()) {
            spdlog::info("Hosting game on port 25565");
        }
    }
    ImGui::Spacing();
    ImGui::TextDisabled("Beitreten:");
    ImGui::SetNextItemWidth(W);
    ImGui::InputText("##IP", m_ConnectIP, sizeof(m_ConnectIP));
    if (ImGui::Button("Connect", ImVec2(W, 28)) && !ctx.network.IsConnected()) {
        if (ctx.network.Connect(m_ConnectIP)) {
            spdlog::info("Connecting to {}", m_ConnectIP);
        }
    }
    if (ctx.network.IsConnected()) {
        if (ImGui::Button("Open Lobby", ImVec2(W, 32))) {
            ctx.scenes.RequestTransition(new LobbyScene());
        }
    }
    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Quit", ImVec2(W, 26))) {
        SDL_Event quitEvent;
        quitEvent.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quitEvent);
    }
    ImGui::TextDisabled("F12 = Entwicklerwerkzeuge");
    ImGui::End();

    // Terrain Generation Debug Window — only visible when dev panels are on.
    if (!DebugUI::IsVisible()) { return; }
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
