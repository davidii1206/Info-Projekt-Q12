/**
 * @file Packets.h
 * @brief Definitions for all network packet structures and types.
 *
 * Every packet is a plain trivially-copyable struct whose first byte is a
 * PacketType tag. Packets are sent verbatim over enet (UDP). Server→Client
 * packets describe authoritative world changes; Client→Server packets carry
 * player intent.
 */

#pragma once
#include <cstdint>

/**
 * @enum PacketType
 * @brief Identifiers for different types of network packets.
 *
 * First byte of every packet must be one of these values.
 */
enum class PacketType : uint8_t {
    PING = 1,             /**< Simple latency check. */

    // Server → Client
    PLAYER_ID_ASSIGN = 2, /**< Assigns a playerId and netId to the local client. */
    PLAYER_JOINED    = 3, /**< Notifies clients that a new player has joined. */
    PLAYER_LEFT      = 4, /**< Notifies clients that a player has left. */
    ENTITY_SNAPSHOT  = 5, /**< Contains updated transform and velocity for an entity. */
    ASSET_JOINED     = 6, /**< Notifies clients that a static asset has been spawned. */
    UNIT_SPAWNED     = 7, /**< Notifies clients that a unit was spawned. */
    UNIT_DIED        = 8, /**< Notifies clients that a unit died. */
    UNIT_HP_UPDATE   = 9, /**< Broadcasts updated HP for a unit. */
    GAME_OVER        = 11,/**< Server broadcasts win/lose condition. */

    // Client → Server
    PLAYER_INPUT     = 10, /**< Sends movement and rotation input from client to server. */
    COMMANDER_ORDER  = 12, /**< Client sends move order to server for selected units. */

    // Server → Client (state sync)
    TERRITORY_SNAPSHOT = 13, /**< Periodic territory zone ownership + progress. */
    FOG_SNAPSHOT       = 14, /**< Periodic fog-of-war grid bits (deprecated, use FOG_DELTA). */
    FOG_DELTA          = 24, /**< Server → Client: changed fog cells since last sync. */
    RESOURCE_SPAWNED   = 15, /**< A resource node was spawned. */
    RESOURCE_DEPLETED  = 16, /**< A resource node was depleted or destroyed. */
    BUILDING_SPAWNED   = 17, /**< A building was spawned. */
    BUILDING_DESTROYED = 18, /**< A building was destroyed. */
    UPGRADE_COMPLETED  = 19, /**< An upgrade path was completed. */
    LOBBY_UPDATE       = 20, /**< Client→Server: player changed bug class or ready state. */
    LOBBY_STATE        = 21, /**< Server→Client: full lobby player list with states. */
    GAME_START         = 22, /**< Server→Client: host started the game. */
    RETURN_TO_LOBBY   = 23, /**< Server→Client: host is returning everyone to lobby. */
    INVENTORY_UPDATE  = 25, /**< Server→Client: per-base resource stockpile sync. */
};

/**
 * @struct PingPacket
 * @brief Simple packet for measuring round-trip time.
 */
struct PingPacket {
    PacketType type      = PacketType::PING; /**< Packet type identifier. */
    uint32_t   timestamp = 0;                 /**< Client-side timestamp. */
};

/**
 * @struct PlayerIdAssignPacket
 * @brief Sent by server to a joining client to assign their identities.
 */
struct PlayerIdAssignPacket {
    PacketType type     = PacketType::PLAYER_ID_ASSIGN; /**< Packet type identifier. */
    uint32_t   playerId = 0;                            /**< The assigned unique player ID. */
    uint32_t   netId    = 0;                            /**< The network ID of the player's entity. */
};

/**
 * @struct PlayerJoinedPacket
 * @brief Broadcast by server when a new player joins.
 */
struct PlayerJoinedPacket {
    PacketType type     = PacketType::PLAYER_JOINED; /**< Packet type identifier. */
    uint32_t   netId    = 0;                         /**< Network ID of the new entity. */
    uint32_t   playerId = 0;                         /**< Player ID of the new player. */
    float      x = 0.f;                              /**< Initial X position. */
    float      y = 0.f;                              /**< Initial Y position. */
    float      z = 0.f;                              /**< Initial Z position. */
};

/**
 * @struct PlayerLeftPacket
 * @brief Broadcast by server when a player disconnects.
 */
struct PlayerLeftPacket {
    PacketType type  = PacketType::PLAYER_LEFT; /**< Packet type identifier. */
    uint32_t   netId = 0;                        /**< Network ID of the removed entity. */
};

/**
 * @struct AssetJoinedPacket
 * @brief Broadcast by server when a static asset (like a model) is added to the world.
 */
struct AssetJoinedPacket {
    PacketType type = PacketType::ASSET_JOINED; /**< Packet type identifier. */
    uint32_t   netId = 0;                       /**< Network ID of the new entity. */
    char       modelPath[128]{};                /**< Path to the asset file. */
    float      x = 0.f;                          /**< Initial X position. */
    float      y = 0.f;                          /**< Initial Y position. */
    float      z = 0.f;                          /**< Initial Z position. */
};

/**
 * @struct EntitySnapshotPacket
 * @brief Regular update from server containing an entity's full transform + velocity.
 */
struct EntitySnapshotPacket {
    PacketType type  = PacketType::ENTITY_SNAPSHOT; /**< Packet type identifier. */
    uint32_t   netId = 0;                            /**< Network ID of the entity. */
    float      x  = 0.f;                             /**< Position X. */
    float      y  = 0.f;                             /**< Position Y. */
    float      z  = 0.f;                             /**< Position Z. */
    float      rx = 0.f;                             /**< Rotation X (Euler degrees). */
    float      ry = 0.f;                             /**< Rotation Y (Euler degrees). */
    float      rz = 0.f;                             /**< Rotation Z (Euler degrees). */
    float      sx = 1.f;                             /**< Scale X. */
    float      sy = 1.f;                             /**< Scale Y. */
    float      sz = 1.f;                             /**< Scale Z. */
    float      vx = 0.f;                             /**< Velocity X. */
    float      vy = 0.f;                             /**< Velocity Y. */
    float      vz = 0.f;                             /**< Velocity Z. */
};

/**
 * @struct PlayerInputPacket
 * @brief Sent by client to server to communicate player intentions.
 */
struct PlayerInputPacket {
    PacketType type = PacketType::PLAYER_INPUT; /**< Packet type identifier. */
    float      dx   = 0.f;                       /**< Horizontal (X) movement input. */
    float      dy   = 0.f;                       /**< Vertical (Y) movement input. */
    float      dz   = 0.f;                       /**< Forward/backward (Z) movement input. */
    float      yaw  = 0.f;                       /**< Current camera yaw. */
    float      pitch = 0.f;                      /**< Current camera pitch. */
};

/**
 * @struct UnitSpawnedPacket
 * @brief Sent when a new unit entity is spawned in the world.
 */
struct UnitSpawnedPacket {
    PacketType type    = PacketType::UNIT_SPAWNED; /**< Packet type identifier. */
    uint32_t   netId   = 0;                        /**< Network ID of the unit. */
    uint32_t   teamId  = 0;                        /**< Owning team. */
    uint8_t    bugClass = 0;                       /**< BugClass enum value (faction/type). */
    uint8_t    role    = 0;                        /**< UnitRole: 0=Combat, 1=Worker. */
    float      x = 0.f;                            /**< Spawn position X. */
    float      y = 0.f;                            /**< Spawn position Y. */
    float      z = 0.f;                            /**< Spawn position Z. */
    float      hp = 100.f;                         /**< Current hit points. */
    float      maxHp = 100.f;                      /**< Maximum hit points. */
};

/**
 * @struct UnitDiedPacket
 * @brief Sent when a unit entity dies.
 */
struct UnitDiedPacket {
    PacketType type  = PacketType::UNIT_DIED; /**< Packet type identifier. */
    uint32_t   netId = 0;                      /**< Network ID of the dead unit. */
};

/**
 * @struct UnitHpUpdatePacket
 * @brief Periodic health sync for a unit.
 */
struct UnitHpUpdatePacket {
    PacketType type  = PacketType::UNIT_HP_UPDATE; /**< Packet type identifier. */
    uint32_t   netId = 0;                          /**< Network ID of the unit. */
    float      hp    = 0.f;                         /**< Current hit points. */
};

/**
 * @struct GameOverPacket
 * @brief Broadcast when the game ends.
 */
struct GameOverPacket {
    PacketType type        = PacketType::GAME_OVER; /**< Packet type identifier. */
    uint32_t   winnerTeam  = 0xFFFFFFFFu; ///< Winning team, or 0xFFFFFFFF = draw/no winner.
};

/**
 * @struct TerritoryZoneData
 * @brief One territory zone's networked state (part of TerritorySnapshotPacket).
 */
struct TerritoryZoneData {
    char     name[32]      = "Zone"; ///< Display name shown in the overlay.
    float    halfW         = 8.f;    ///< Half-width of the zone along X.
    float    halfD         = 8.f;    ///< Half-depth of the zone along Z.
    float    captureTime   = 10.f;   ///< Seconds of dominance required to capture.
    float    captureProgress = 0.f;  ///< Current capture progress in seconds.
    uint32_t ownerTeam     = 0xFFFF'FFFFu; ///< Owning team, or 0xFFFFFFFF if neutral.
    uint32_t contestedBy   = 0xFFFF'FFFFu; ///< Team currently ahead in capture, if any.
    float    centerX = 0.f;          ///< Zone centre X (world space).
    float    centerY = 0.f;          ///< Zone centre Y (world space).
    float    centerZ = 0.f;          ///< Zone centre Z (world space).
};

/**
 * @struct TerritorySnapshotPacket
 * @brief Server → Client: state of all territory zones.
 *
 * Sent periodically (e.g. 2 Hz) so clients can render the territory overlay
 * without needing direct access to the server registry.
 */
struct TerritorySnapshotPacket {
    PacketType type      = PacketType::TERRITORY_SNAPSHOT; /**< Packet type identifier. */
    uint32_t   zoneCount = 0;                              /**< Number of valid entries in zones[]. */
    TerritoryZoneData zones[8]{};                          /**< Per-zone state (up to 8 zones). */
};

/**
 * @struct FogSnapshotPacket
 * @brief Server → Client: bit-packed revealed-cell grid for the fog of war.
 *
 * The grid is serialised as an array of uint64_t words.  Bit i of word w
 * corresponds to cell index w*64+i (row-major: z*cellsX + x).
 */
struct FogSnapshotPacket {
    PacketType type    = PacketType::FOG_SNAPSHOT; /**< Packet type identifier. */
    uint16_t   cellsX  = 0;                         /**< Number of fog cells along X. */
    uint16_t   cellsZ  = 0;                         /**< Number of fog cells along Z. */
    /// Bit-packed revealed flags; enough for a 375×375 grid (140 625 bits → 2198 uint64s).
    uint64_t   gridData[2200]{};
};

/**
 * @struct FogDeltaPacket
 * @brief Server → Client: only the cells that changed since the last sync.
 *
 * Instead of sending the full 375×375 bit grid every time, this packet
 * carries a list of newly-revealed cell indices (row-major).  For the
 * common case of a few units moving, this is < 100 bytes instead of ~18 KB.
 */
struct FogDeltaPacket {
    PacketType type   = PacketType::FOG_DELTA; /**< Packet type identifier. */
    uint16_t   count  = 0;                     /**< Number of valid entries in cells[]. */
    /// Newly-revealed cell indices (row-major: z*cellsX + x).
    uint32_t   cells[512]{};
};

/**
 * @struct ResourceSpawnedPacket
 * @brief Server → Client: a resource node appeared (permanent spawn or meat drop).
 */
struct ResourceSpawnedPacket {
    PacketType type     = PacketType::RESOURCE_SPAWNED; /**< Packet type identifier. */
    uint32_t   netId    = 0;                            /**< Network ID of the resource node. */
    uint8_t    resourceType = 1;                        ///< ResourceType enum value.
    float      x = 0.f;                                 /**< Position X. */
    float      y = 0.f;                                 /**< Position Y. */
    float      z = 0.f;                                 /**< Position Z. */
};

/**
 * @struct ResourceDepletedPacket
 * @brief Server → Client: a resource node has been depleted or destroyed.
 */
struct ResourceDepletedPacket {
    PacketType type  = PacketType::RESOURCE_DEPLETED; /**< Packet type identifier. */
    uint32_t   netId = 0;                             /**< Network ID of the depleted node. */
};

/**
 * @struct InventoryUpdatePacket
 * @brief Server → Client: per-base resource stockpile snapshot.
 *
 * Sent at 2 Hz for every base entity that has a ResourceInventory. The client
 * mirrors these into a small map keyed by baseNetId so HUDs and UI on the
 * joined player can display the same numbers the server sees.
 */
struct InventoryUpdatePacket {
    PacketType type      = PacketType::INVENTORY_UPDATE; /**< Packet type identifier. */
    uint32_t   baseNetId = 0;  /**< Network ID of the base entity holding this inventory. */
    uint32_t   teamId    = 0;  /**< Team that owns the base (for HUD filtering). */
    int32_t    pilze     = 0;
    int32_t    beeren    = 0;
    int32_t    nektar    = 0;
    int32_t    samen     = 0;
    int32_t    insekten  = 0;
    int32_t    fleisch   = 0;
    int32_t    holz      = 0;
};

/**
 * @struct CommanderOrderPacket
 * @brief Client → Server: move or attack order for selected units.
 *
 * For move orders (orderType == 0), x/y/z is the destination and
 * targetNetId is unused.  For attack orders (orderType == 1 for unit,
 * 2 for building), targetNetId identifies the enemy entity and x/y/z
 * is the target's current position (used as initial destination).
 */
struct CommanderOrderPacket {
    PacketType type          = PacketType::COMMANDER_ORDER; /**< Packet type identifier. */
    uint32_t   playerId      = 0;                           /**< Issuing player's ID. */
    float      x = 0.f;                                     ///< World destination X.
    float      y = 0.f;                                     ///< World destination Y.
    float      z = 0.f;                                     ///< World destination Z.
    uint32_t   selectedCount = 0;                           ///< How many units are selected (max 32).
    uint32_t   selectedNetIds[32]{};                        ///< NetIds of selected units.
    uint8_t    orderType     = 0;                           ///< 0=Move, 1=AttackUnit, 2=AttackBuilding.
    uint32_t   targetNetId   = 0;                           ///< Network ID of attack target (0 for move orders).
};

/**
 * @struct BuildingSpawnedPacket
 * @brief Server → Client: a building was constructed.
 */
struct BuildingSpawnedPacket {
    PacketType type    = PacketType::BUILDING_SPAWNED; /**< Packet type identifier. */
    uint32_t   netId   = 0;                            /**< Network ID of the building. */
    uint32_t   teamId  = 0;                            /**< Owning team. */
    uint8_t    buildingType = 0;                       ///< BuildingType enum value.
    uint8_t    specialType  = 0;                       ///< SpecialBuildingType enum value (for Special buildings).
    uint32_t   tier    = 1;                            /**< Building tier/level. */
    float      x = 0.f;                                /**< Position X. */
    float      y = 0.f;                                /**< Position Y (terrain surface height). */
    float      z = 0.f;                                /**< Position Z. */
    float      hp = 500.f;                             /**< Current hit points. */
    float      maxHp = 500.f;                          /**< Maximum hit points. */
};

/**
 * @struct BuildingDestroyedPacket
 * @brief Server → Client: a building was destroyed.
 */
struct BuildingDestroyedPacket {
    PacketType type  = PacketType::BUILDING_DESTROYED; /**< Packet type identifier. */
    uint32_t   netId = 0;                              /**< Network ID of the destroyed building. */
};

/**
 * @struct UpgradeCompletedPacket
 * @brief Server → Client: a team completed an upgrade path.
 */
struct UpgradeCompletedPacket {
    PacketType type   = PacketType::UPGRADE_COMPLETED; /**< Packet type identifier. */
    uint32_t   pathId = 0;    ///< UpgradePathID that was completed.
    uint32_t   teamId = 0;    ///< Which team completed it.
};

/**
 * @struct LobbyUpdatePacket
 * @brief Client → Server: player changed bug class or ready state in lobby.
 */
struct LobbyUpdatePacket {
    PacketType type      = PacketType::LOBBY_UPDATE; /**< Packet type identifier. */
    uint32_t   playerId  = 0;                        /**< Sender's player ID. */
    uint8_t    bugClass  = 0;                        /**< BugClass enum value. */
    uint8_t    ready     = 0;                        /**< 1 = ready, 0 = not ready. */
};

/**
 * @struct LobbyStatePacket
 * @brief Server → Client: full lobby player list with states.
 */
struct LobbyStatePacket {
    static constexpr uint32_t MAX_PLAYERS = 16;

    PacketType type        = PacketType::LOBBY_STATE; /**< Packet type identifier. */
    uint32_t   playerCount = 0;                       /**< Number of valid entries in players[]. */

    struct PlayerInfo {
        uint32_t playerId  = 0;  /**< Player ID. */
        uint32_t netId     = 0;  /**< Player's network entity ID. */
        uint8_t  bugClass  = 0;  /**< BugClass enum value (0 = None). */
        uint8_t  ready     = 0;  /**< 1 = ready, 0 = not ready. */
    } players[MAX_PLAYERS]{};
};

/**
 * @struct GameStartPacket
 * @brief Server → Client: host started the game. Everyone transitions to GameScene.
 */
struct GameStartPacket {
    PacketType type = PacketType::GAME_START; /**< Packet type identifier. */
};

/**
 * @struct ReturnToLobbyPacket
 * @brief Server → Client: host is returning everyone to the lobby.
 */
struct ReturnToLobbyPacket {
    PacketType type = PacketType::RETURN_TO_LOBBY; /**< Packet type identifier. */
};
