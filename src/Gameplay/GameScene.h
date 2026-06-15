/**
 * @file GameScene.h
 * @brief Header for the main gameplay scene.
 */

#pragma once
#include "Scene.h"
#include "../Graphics/Camera.h"
#include "../Graphics/GlobalUniforms.h"
#include "../Graphics/API/Framebuffer.h"
#include "../Graphics/API/GPUBuffer.h"
#include "../Core/PhysicsServer.h"
#include "ResourceManager.h"
#include "ResourceSystem.h"
#include "ResourceHUD.h"
#include "FogOfWar.h"
#include "TerritorySystem.h"
#include "ScatterSystem.h"
#include "HUDTextureRegistry.h"
#include "../Core/WorldManager.h"
#include <unordered_map>
#include <vector>
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
     * @brief Performs per-frame logic updates (input, camera).
     * @param ctx The scene context.
     * @param dt Delta time.
     */
    void LogicUpdate(SceneContext& ctx, float dt) override;

    /**
     * @brief Performs per-frame UI updates (ImGui).
     * @param ctx The scene context.
     * @param dt Delta time.
     */
    void UIUpdate(SceneContext& ctx, float dt) override;

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

    /**
     * @brief Spawns a physics-driven cube in the world (Server only).
     * @param ctx The scene context.
     * @param pos Initial position.
     */
    void SpawnPhysicsCube(SceneContext& ctx, glm::vec3 pos);

    /// Handles for static mesh collision bodies (scene geometry).
    /// Stored so they can be removed on OnExit().
    std::vector<PhysicsBodyHandle> m_MeshCollisionBodies;

    /// Server-side resource manager: owns spawn points, depletion, respawn.
    ResourceManager m_ResourceManager;

    /// Fog of War grid – tracks which map cells have been explored.
    FogGrid m_Fog;

    /// Procedural world data (heightmap + biomes) used to drive decorative
    /// prop scattering. Generated locally on every peer from the shared seed.
    WorldManager m_World;

    /// Seed shared by all peers so the scatter field is identical everywhere.
    /// Keep this in sync with whatever seed drives your actual terrain.
    uint32_t m_WorldSeed = 12345;

    /// Whether the map overlay (Fog + Territory) is currently visible.
    /// Toggled by the "Karte" button in the Game window.
    bool m_ShowMapOverlay = false;

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

    // --- Shadow Map ---
    /// Vertex shader for the depth-only shadow pass.
    std::unique_ptr<Shader> m_ShadowVertShader;
    /// Fragment shader for the depth-only shadow pass.
    std::unique_ptr<Shader> m_ShadowFragShader;
    /// Graphics pipeline for the shadow pass.
    GraphicsPipeline* m_ShadowPipeline = nullptr;
    /// Depth-only framebuffer rendered from the sun's perspective.
    std::unique_ptr<Framebuffer> m_ShadowMap;
    /// GPU buffer holding only the sunVP matrix for the shadow pass.
    std::unique_ptr<GPUBuffer> m_ShadowUBO;


    // Sonnenlicht — schräg von oben (Mittag, leicht südwestlich)
    glm::vec3 m_SunDirection     = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
    glm::vec3 m_SunColor         = {1.0f, 0.97f, 0.88f};  // warmes Tageslicht
    float     m_SunIntensity     = 1.0f;

    // Ambient — bläuliches Himmelslicht, Schatten nicht pechschwarz
    glm::vec3 m_AmbientColor     = {0.45f, 0.60f, 0.90f};
    float     m_AmbientIntensity = 0.25f;

    // Shadow tuning — adjustable at runtime via the "Shadow Debug" ImGui window.
    float m_ShadowBiasConstant = 0.1f;  ///< Uniform depth offset (world-unit scale)
    float m_ShadowBiasSlope    = 0.25f; ///< Extra offset for grazing-angle surfaces
    float m_ShadowOrthoSize    = 40.0f; ///< Half-size of the orthographic shadow frustum

    // Cached values to detect when the shadow pipeline needs to be rebuilt
    float m_LastShadowBiasConstant = -1.0f;
    float m_LastShadowBiasSlope    = -1.0f;
};
