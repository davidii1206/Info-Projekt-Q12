/**
 * @file GameScene.h
 * @brief Header for the main gameplay scene.
 */

#pragma once
#include "Scene.h"
#include "../Graphics/Camera.h"
#include "../Graphics/GlobalUniforms.h"
#include <unordered_map>
#include <cstdint>
#include <memory>

class Shader;
class GraphicsPipeline;

/**
 * @class GameScene
 * @brief The main gameplay scene.
 * 
 * Handles networked game logic, player movement, and 3D rendering of the game world.
 */
class GameScene : public IScene {
public:
    /**
     * @brief Constructs the GameScene.
     */
    GameScene();

    /**
     * @brief Destroys the GameScene and its resources.
     */
    ~GameScene() override;

    /**
     * @brief Gets the name of the scene.
     * @return The scene name.
     */
    const char* Name() const override { return "GameScene"; }

    /**
     * @brief Called when the scene is entered.
     * @param ctx The scene context.
     */
    void OnEnter(SceneContext& ctx) override;

    /**
     * @brief Called when the scene is exited.
     * @param ctx The scene context.
     */
    void OnExit(SceneContext& ctx)  override;

    /**
     * @brief Performs per-frame updates (UI, input, camera).
     * @param ctx The scene context.
     * @param dt Delta time.
     */
    void FrameUpdate(SceneContext& ctx, float dt) override;

    /**
     * @brief Performs fixed-rate updates (networking, physics).
     * @param ctx The scene context.
     * @param dt Fixed delta time.
     */
    void FixedUpdate(SceneContext& ctx, float dt) override;

    /**
     * @brief Renders the game world.
     * @param ctx The scene context.
     * @param renderer Pointer to the renderer.
     */
    void Render(SceneContext& ctx, Renderer* renderer) override;

private:
    /**
     * @brief Polls for new player connections and disconnections (Server only).
     * @param ctx The scene context.
     */
    void PollConnectionEvents(SceneContext& ctx);

    /**
     * @brief Processes incoming movement packets from clients (Server only).
     * @param ctx The scene context.
     */
    void PollClientPackets(SceneContext& ctx);

    /**
     * @brief Broadcasts entity position/velocity snapshots to all clients (Server only).
     * @param ctx The scene context.
     */
    void SendSnapshots(SceneContext& ctx);

    /**
     * @brief Processes incoming state updates from the server (Client only).
     * @param ctx The scene context.
     */
    void PollServerPackets(SceneContext& ctx);

    /**
     * @brief Sends local player input to the server (Client only).
     * @param ctx The scene context.
     */
    void SendLocalInput(SceneContext& ctx);

    // --- Server state ---
    /// ID for the next networked entity.
    uint32_t m_NextNetId    = 1;
    /// ID for the next player.
    uint32_t m_NextPlayerId = 0;
    /// Mapping from network ID to server-side entity.
    std::unordered_map<uint32_t, entt::entity> m_ServerNetMap;
    /// Mapping from peer ID to network ID.
    std::unordered_map<uint32_t, uint32_t>     m_PeerToNetId;

    // --- Client state ---
    /// Local player's player ID.
    uint32_t m_MyPlayerId = 0;
    /// Local player's network ID.
    uint32_t m_MyNetId    = 0;
    /// Whether the player ID has been assigned by the server.
    bool     m_IdAssigned = false;
    /// Mapping from network ID to client-side entity.
    std::unordered_map<uint32_t, entt::entity> m_ClientNetMap;

    /// Accumulator for snapshot broadcasting.
    float m_SnapAccum = 0.f;
    /// Rate at which snapshots are sent (20 Hz).
    static constexpr float SNAPSHOT_RATE = 1.f / 20.f;

    // --- Rendering ---
    /// Vertex shader for models.
    std::unique_ptr<Shader> m_VertShader;
    /// Fragment shader for models.
    std::unique_ptr<Shader> m_FragShader;
    /// Graphics pipeline for model rendering.
    GraphicsPipeline* m_ModelPipeline = nullptr;
    /// Main game camera.
    std::unique_ptr<Camera> m_Camera;
    /// Total time elapsed in the scene.
    float m_TotalTime = 0.0f;
    /// Total number of frames rendered.
    uint32_t m_FrameCount = 0;
    /// Whether free-fly camera mode is active.
    bool m_FreeFly = false;


    // Sonnenlicht — schräg von oben (Mittag, leicht südwestlich)
    glm::vec3 m_SunDirection     = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
    glm::vec3 m_SunColor         = {1.0f, 0.97f, 0.88f};  // warmes Tageslicht
    float     m_SunIntensity     = 3.5f;

    // Ambient — bläuliches Himmelslicht, Schatten nicht pechschwarz
    glm::vec3 m_AmbientColor     = {0.45f, 0.60f, 0.90f};
    float     m_AmbientIntensity = 0.25f;
};