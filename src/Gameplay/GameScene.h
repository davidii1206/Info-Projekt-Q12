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
#include <unordered_set>
#include "ScatterSystem.h"
#include "HUDTextureRegistry.h"
#include "../Core/WorldManager.h"
#include "UpgradeSystem.h"
#include "../Networking/Packets.h"
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <memory>
#include <limits>

class Shader;
class GraphicsPipeline;

/// One instanced draw batch: all scatter entities sharing the same model path
/// and the same GLTF mesh node are collapsed into a single DrawIndexed call.
struct ScatterBatch {
    std::string             modelPath;       ///< GLB path shared by all instances in this batch.
    uint32_t                meshInstanceIdx; ///< Index into SceneData::meshInstances
    uint32_t                instanceCount;   ///< Number of instances in this batch.
    std::unique_ptr<GPUBuffer> instanceBuffer; ///< N × mat4, entityMat * meshNodeTransform
};

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
    void SpawnUnit(SceneContext& ctx, uint32_t teamId, glm::vec3 pos, BugClass bugClass, int tier = 1, float hp = -1.f);

    /** @brief Spawns a worker (scaled-down collector unit) on the server and broadcasts to clients. */
    void SpawnWorker(SceneContext& ctx, uint32_t teamId, glm::vec3 pos, BugClass bugClass, float hp = 50.f);

    /** @brief Spawns a collector unit (unit + CollectorComponent) on the server. */
    void SpawnCollector(SceneContext& ctx, uint32_t teamId, glm::vec3 pos);

    /** @brief Spawns a building on the server and broadcasts to clients. */
    entt::entity SpawnBuilding(SceneContext& ctx, BuildingType type, uint32_t teamId,
                               glm::vec3 pos, uint32_t tier = 1,
                               const std::string& model = "assets/cube.glb",
                               SpecialBuildingType specialType = SpecialBuildingType::NectarRefinery);

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

    /** @brief Server: broadcast per-base resource inventory snapshots to all clients. */
    void SendInventoryUpdates(SceneContext& ctx);

    /** @brief Server: assign netIds to resource nodes and broadcast spawns. */
    void SyncResourceSpawns(SceneContext& ctx);

    /** @brief Client: process a received TerritorySnapshotPacket. */
    void HandleTerritorySnapshot(const TerritorySnapshotPacket& pkt);

    /** @brief Client: process a received FogSnapshotPacket. */
    void HandleFogSnapshot(const FogSnapshotPacket& pkt);

    /** @brief Client: draw territory overlay from synced packet data (no server registry needed). */

    /** @brief Commander: unproject screen pos to XZ plane in world space. */
    glm::vec3 ScreenToWorldXZ(float sx, float sy, int winW, int winH);

    /** @brief Draw HP bars above units via ImGui overlay. */
    void DrawUnitHPBars(SceneContext& ctx);

    /** @brief Enter building placement mode for the given type. */
    void EnterPlacementMode(SceneContext& ctx, BuildingType type);
    /** @brief Cancel placement mode and destroy the ghost entity. */
    void CancelPlacement(SceneContext& ctx);

    /// Builds per-model instance transform SSBOs for all scatter entities.
    /// Called once in OnEnter() after scatter population; results are reused every frame.
    void BuildScatterBatches(SceneContext& ctx, const FogGrid* fog = nullptr);

    /// Returns the fog grid for the local player (host: playerId's grid, client: m_ClientFog).
    const FogGrid& LocalFog(const SceneContext& ctx) const;

    /// Helper: checks whether any tile in a chunk is revealed in the given fog grid.
    bool IsChunkRevealed(const FogGrid& fog, int chunkX, int chunkZ) const;

    /// Generates/re-generates the top-down map texture from world data and fog state.
    void GenerateMapTexture(SceneContext& ctx);

    /** @brief Server: apply an upgrade path for a team and broadcast it. */
    void ApplyUpgrade(SceneContext& ctx, uint32_t teamId, UpgradePathID pathId);

    /// Handles for static mesh collision bodies (scene geometry).
    /// Stored so they can be removed on OnExit().
    std::vector<PhysicsBodyHandle> m_MeshCollisionBodies;

    /// Server-side resource manager: owns spawn points, depletion, respawn.
    ResourceManager m_ResourceManager;

    /// Per-team fog grids on the server. Keyed by teamId (= playerId in FFA).
    std::unordered_map<uint32_t, FogGrid> m_TeamFogs;

    /// Procedural world data (heightmap + biomes) used to drive decorative
    /// prop scattering. Generated locally on every peer from the shared seed.
    WorldManager m_World;

    /// Seed shared by all peers so the scatter field is identical everywhere.
    /// Keep this in sync with whatever seed drives your actual terrain.
    uint32_t m_WorldSeed = 12345;

    /// Whether the map overlay (Fog + Territory) is currently visible.
    /// Toggled by the "Karte" button in the Game window.
    bool m_ShowMapOverlay = false;

    // --- Selection circle drag state (Pikmin-style) ---
    bool     m_SelectDragging = false;       ///< Left mouse held during drag.
    glm::vec2 m_SelectStartWorld{0.f, 0.f}; ///< World XZ at drag start.
    glm::vec2 m_SelectEndWorld{0.f, 0.f};   ///< World XZ at current mouse.
    float    m_SelectMaxRadius = 40.f;       ///< Cap the selection circle at this size.

    // --- Camera Modes ---
    /// @brief Active camera/control mode.
    enum class CameraMode { Commander, Building };
    CameraMode m_CameraMode = CameraMode::Building; ///< Current camera mode.
    /// Saved Commander camera XZ position (restored when switching back from Building).
    glm::vec3 m_SavedCommanderPos{0.f};
    /// Default height above ground for Commander and Building modes.
    float m_TopDownHeight = 40.f;
    /// Orthographic zoom level for Building mode (half-height of frustum).
    float m_BuildOrthoSize = 30.f;
    /// Yaw angle for isometric mode (rotated with left/right arrow keys).
    float m_BuildYaw = -135.f;
    /// Starting yaw of the current snap animation.
    float m_BuildYawFrom = -135.f;
    /// Target yaw of the current snap animation.
    float m_BuildYawTarget = -135.f;
    /// Animation progress [0..1] between m_BuildYawFrom and m_BuildYawTarget.
    float m_YawAnimT = 1.f;
    /// Camera position at the start of the snap animation.
    glm::vec3 m_CamPosFrom{0.f};
    /// World-space pivot (center of screen on ground) for orbit rotation.
    glm::vec3 m_YawPivot{0.f};

    /// Client-side entity for building placement ghost (transparent preview).
    entt::entity m_GhostEntity = entt::null;
    /// Currently selected building type for placement (-1 = none, <0 = special building).
    int m_SelectedBuildingType = -1;
    /// When placing a special building, which one.
    SpecialBuildingType m_SelectedSpecialBuilding = SpecialBuildingType::NectarRefinery;
    /// Whether the player is in placement hover mode.
    bool m_PlacementActive = false;
    /// Current ghost world position (tile-snapped, updated every frame during placement).
    glm::vec3 m_GhostPos{0.f};
    /// Raw cursor world position (unsnapped) — used as tree-fade center so the
    /// fade follows the mouse smoothly instead of jumping between tile centres.
    glm::vec3 m_FadeCenter{0.f};
    /// Whether the current placement target is valid (buildable, not blocked).
    bool m_PlacementValid = false;

    // --- Unit system ---
    /// Network IDs of units currently selected by this client's Commander.
    std::vector<uint32_t> m_SelectedUnits;

    struct VisualExplosion {
        glm::vec3 position;
        float timer = 0.f;
        float maxDuration = 0.5f;
        float maxRadius = 2.0f;
        int type = 0; // 0 = Fire/Kamikaze, 1 = Toxic Hairs (purple/green), 2 = Sleep Pollen (blue/cyan), 3 = Healing (emerald green)
    };
    std::vector<VisualExplosion> m_VisualExplosions;

    struct SlimeNode {
        glm::vec3 position;
        uint32_t teamId;
        float timer;
    };
    std::vector<SlimeNode> m_ServerSlimeNodes;
    std::vector<SlimeNode> m_ClientSlimeNodes;
    float m_SlimeDropAccum = 0.f;
    /// Direct control mode status
    bool                  m_DirectControlActive = false;
    /// Network ID of the unit under direct control
    uint32_t              m_DirectControlNetId  = 0;
    /// Whether the game has ended.
    bool     m_GameOver      = false;
    uint32_t m_WinnerTeam    = 0xFFFFFFFFu; ///< Winning team once the game is over (0xFFFFFFFF = none).

    /// Teams that have ever had a Main building (used to detect elimination).
    std::unordered_set<uint32_t> m_TeamsWithMainBuildings;

    /// Timer counting up after game over; triggers auto-return to lobby.
    float m_GameOverTimer = 0.f;
    static constexpr float GAME_OVER_DELAY = 8.f; ///< Seconds before auto-return to lobby.

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
    static constexpr float TERRITORY_SNAP_RATE = 1.f / 2.f; ///< Territory snapshot rate (2 Hz).

    /// Accumulator for fog delta broadcasting (now used for FOG_DELTA packets).
    float m_FogSnapAccum = 0.f;
    static constexpr float FOG_SNAP_RATE = 1.f / 4.f; ///< Fog delta rate (4 Hz).

    /// Accumulator for inventory snapshot broadcasting (2 Hz, per-base stockpile).
    float m_InventorySnapAccum = 0.f;
    static constexpr float INVENTORY_SNAP_RATE = 1.f / 2.f; ///< Inventory snap rate (2 Hz).

    /// Per-base inventory mirror, populated from INVENTORY_UPDATE packets on
    /// every recipient (host included, so the HUD has one read path).
    std::unordered_map<uint32_t, ResourceInventory> m_ClientBaseInventories;

    // --- Client-side synced state ---
    /// Client-side fog grid copy (updated from server snapshot).
    FogGrid m_ClientFog;
    bool    m_HasFogData = false; ///< True once a fog snapshot has been received.

    /// Client-side territory zone cache (updated from server snapshot).
    struct ClientTerritoryZone {
        char     name[32]{};            ///< Zone display name.
        float    halfW = 8.f;           ///< Zone half-extent along X.
        float    halfD = 8.f;           ///< Zone half-extent along Z.
        float    captureProgress = 0.f; ///< Current capture progress (seconds).
        float    captureTime = 10.f;    ///< Capture time required (seconds).
        uint32_t ownerTeam = 0xFFFF'FFFFu;        ///< Owning team (0xFFFFFFFF = neutral).
        uint32_t contestedBy = 0xFFFF'FFFFu;      ///< Team currently contesting, if any.
        glm::vec3 center{0.f};                     ///< Zone centre (world space).
    };
    std::vector<ClientTerritoryZone> m_ClientTerritories; ///< Synced territory cache (future use).

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
    glm::vec3 m_SunDirection     = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f)); ///< Sun direction.
    glm::vec3 m_SunColor         = {1.0f, 0.97f, 0.88f};  ///< Sun colour (warm daylight).
    float     m_SunIntensity     = 1.0f;                  ///< Sun intensity.

    // Ambient — bläuliches Himmelslicht, Schatten nicht pechschwarz
    glm::vec3 m_AmbientColor     = {0.45f, 0.60f, 0.90f}; ///< Ambient sky colour.
    float     m_AmbientIntensity = 0.25f;                 ///< Ambient intensity.

    // Shadow tuning — adjustable at runtime via the "Shadow Debug" ImGui window.
    float m_ShadowBiasConstant = 0.1f;  ///< Uniform depth offset (world-unit scale)
    float m_ShadowBiasSlope    = 0.25f; ///< Extra offset for grazing-angle surfaces
    float m_ShadowOrthoSize    = 40.0f; ///< Half-size of the orthographic shadow frustum

    // Cached values to detect when the shadow pipeline needs to be rebuilt
    float m_LastShadowBiasConstant = -1.0f; ///< Last applied bias constant (rebuild trigger).
    float m_LastShadowBiasSlope    = -1.0f; ///< Last applied bias slope (rebuild trigger).

    // --- GPU Instancing ---
    /// Pre-built batches (one per unique model path × GLTF mesh node). Built once in OnEnter().
    std::vector<ScatterBatch>  m_ScatterBatches;
    /// Single identity mat4 SSBO bound for non-scatter draw calls so slot 1 is always valid.
    std::unique_ptr<GPUBuffer> m_IdentityInstanceBuffer;

    // --- Per-chunk terrain mesh metadata ---
    int  m_ChunkSize       = 32;       ///< Tiles per chunk edge.
    int  m_ChunksPerAxis   = 0;        ///< Chunks along one grid edge.

    bool m_ScatterBatchesDirty = false;
    /// Separate timer for scatter batch rebuilds (avoids sharing with fog cover).
    float        m_ScatterBatchTimer = 0.f;
    static constexpr float SCATTER_BATCH_REBUILD_DELAY = 0.4f; ///< Throttle: rebuild scatter batches after fog change.

    // --- Map Texture (top-down view of the world from generator data) ---
    std::unique_ptr<class Texture> m_MapTexture;
    bool m_MapTextureDirty = true;

    // --- Per-tile fog of war GPU texture ---
    std::shared_ptr<class Texture> m_FogTexture;
    bool m_FogTextureDirty = true;

    void UpdateFogTexture(const SceneContext& ctx, const FogGrid& fog);
    void EnsureFogTexture(const SceneContext& ctx);
};
