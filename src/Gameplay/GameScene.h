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
#include "HUDTextureRegistry.h"
#include "../Networking/Packets.h"
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

    /**
     * @brief Loads the scene GLTF and registers its geometry as static mesh collision.
     *
     * Called once from OnEnter() on the server.  Extracts the CPU-side vertex/index
     * data from AssetManager (cached from the same LoadGLTF call used for rendering)
     * and passes it to MeshCollisionBuilder + PhysicsServer::AddStaticMesh().
     *
     * @param ctx     The scene context (provides access to physics).
     * @param glbPath Path to the .glb scene file (same as the visual model path).
     * @param transform Optional world transform to bake into the physics vertices.
     *                  Defaults to identity (no transform).
     */
    void LoadSceneMeshCollision(
        SceneContext&     ctx,
        const std::string& glbPath,
        const glm::mat4&   transform = glm::mat4(1.f));

    // -----------------------------------------------------------------------
    // Gameplay systems
    // -----------------------------------------------------------------------

    /** @brief Spawns a unit entity on the server and broadcasts to clients. */
    void SpawnUnit(SceneContext& ctx, uint32_t teamId, glm::vec3 pos, float hp = 100.f);

    /** @brief Spawns a building on the server and broadcasts to clients. */
    entt::entity SpawnBuilding(SceneContext& ctx, BuildingType type, uint32_t teamId,
                               glm::vec3 pos, uint32_t tier = 1,
                               const std::string& model = "assets/cube.glb");

    /** @brief Handle destruction of a building: broadcast, remove. */
    void HandleBuildingDeath(SceneContext& ctx, entt::entity entity, uint32_t netId);

    /** @brief Server: pick a random territory spawn point for a team. */
    glm::vec3 RandomSpawnInTerritory(SceneContext& ctx, uint32_t teamId);

    /** @brief Server tick: auto-combat – units attack nearest enemies. */
    void UpdateCombat(SceneContext& ctx, float dt);

    /** @brief Server tick: move units toward their commander order destination. */
    void UpdateUnitMovement(SceneContext& ctx, float dt);

    /** @brief Server tick: check win condition (all enemy bases destroyed). */
    void CheckWinCondition(SceneContext& ctx);

    /** @brief Handle death of a unit: drop resource, broadcast, remove. */
    void HandleUnitDeath(SceneContext& ctx, entt::entity entity, uint32_t netId);

    /** @brief Server: broadcast territory zone state to all clients. */
    void SendTerritorySnapshot(SceneContext& ctx);

    /** @brief Server: broadcast fog-of-war grid to all clients. */
    void SendFogSnapshot(SceneContext& ctx);

    /** @brief Server: assign netIds to resource nodes and broadcast spawns. */
    void SyncResourceSpawns(SceneContext& ctx);

    /** @brief Client: process a received TerritorySnapshotPacket. */
    void HandleTerritorySnapshot(const TerritorySnapshotPacket& pkt);

    /** @brief Client: process a received FogSnapshotPacket. */
    void HandleFogSnapshot(const FogSnapshotPacket& pkt);

    /** @brief Client: draw territory overlay from synced packet data (no server registry needed). */
    void DrawClientTerritoryOverlay(ImVec2 mapOriginPx, ImVec2 mapSizePx);

    /** @brief Commander: unproject screen pos to XZ plane in world space. */
    glm::vec3 ScreenToWorldXZ(float sx, float sy, int winW, int winH);

    /** @brief Draw HP bars above units via ImGui overlay. */
    void DrawUnitHPBars(SceneContext& ctx);

    /** @brief Enter building placement mode for the given type. */
    void EnterPlacementMode(SceneContext& ctx, BuildingType type);
    /** @brief Cancel placement mode and destroy the ghost entity. */
    void CancelPlacement(SceneContext& ctx);

    /// Handles for static mesh collision bodies (scene geometry).
    /// Stored so they can be removed on OnExit().
    std::vector<PhysicsBodyHandle> m_MeshCollisionBodies;

    /// Server-side resource manager: owns spawn points, depletion, respawn.
    ResourceManager m_ResourceManager;

    /// Fog of War grid – tracks which map cells have been explored.
    FogGrid m_Fog;

    /// Whether the map overlay (Fog + Territory) is currently visible.
    /// Toggled by the "Karte" button in the Game window.
    bool m_ShowMapOverlay = false;

    // --- Camera Modes ---
    enum class CameraMode { Exploring, Commander, Building };
    CameraMode m_CameraMode = CameraMode::Exploring;
    /// Saved 1st-person camera state (restored when leaving Commander/Building).
    glm::vec3 m_SavedExplorePos{0.f};
    float     m_SavedExploreYaw   = 0.f;
    float     m_SavedExplorePitch = 0.f;
    /// Saved Commander camera XZ position (restored when switching back from Building).
    glm::vec3 m_SavedCommanderPos{0.f};
    /// Default height above ground for Commander and Building modes.
    float m_TopDownHeight = 40.f;
    /// Orthographic zoom level for Building mode (half-height of frustum).
    float m_BuildOrthoSize = 30.f;

    /// Client-side entity for building placement ghost (transparent preview).
    entt::entity m_GhostEntity = entt::null;
    /// Currently selected building type for placement (-1 = none).
    int m_SelectedBuildingType = -1;
    /// Whether the player is in placement hover mode.
    bool m_PlacementActive = false;

    // --- Unit system ---
    /// Network IDs of units currently selected by this client's Commander.
    std::vector<uint32_t> m_SelectedUnits;
    /// Whether the game has ended.
    bool     m_GameOver      = false;
    uint32_t m_WinnerTeam    = 0xFFFFFFFFu;

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

    /// Accumulator for territory snapshot broadcasting (2 Hz).
    float m_TerritorySnapAccum = 0.f;
    static constexpr float TERRITORY_SNAP_RATE = 1.f / 2.f;

    /// Accumulator for fog snapshot broadcasting (2 Hz).
    float m_FogSnapAccum = 0.f;
    static constexpr float FOG_SNAP_RATE = 1.f / 2.f;

    // --- Client-side synced state ---
    /// Client-side fog grid copy (updated from server snapshot).
    FogGrid m_ClientFog;
    bool    m_HasFogData = false;

    /// Client-side territory zone cache (updated from server snapshot).
    struct ClientTerritoryZone {
        char     name[32]{};
        float    halfW = 8.f, halfD = 8.f;
        float    captureProgress = 0.f, captureTime = 10.f;
        uint32_t ownerTeam = 0xFFFF'FFFFu;
        uint32_t contestedBy = 0xFFFF'FFFFu;
        glm::vec3 center{0.f};
    };
    std::vector<ClientTerritoryZone> m_ClientTerritories;
    bool m_HasTerritoryData = false;

    // --- Resource networking ---
    /// Tracks resource entities on server for netId assignment and broadcast.
    std::unordered_map<uint32_t, entt::entity> m_ResourceNetMap;

    // --- Rendering ---
    /// Vertex shader for models.
    std::unique_ptr<Shader> m_VertShader;
    /// Fragment shader for models.
    std::unique_ptr<Shader> m_FragShader;
    /// Graphics pipeline for model rendering.
    GraphicsPipeline* m_ModelPipeline = nullptr;
    /// Transparent pipeline for building-placement ghost.
    GraphicsPipeline* m_GhostPipeline = nullptr;
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
