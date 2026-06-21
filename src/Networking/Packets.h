/**
 * @file Packets.h
 * @brief Definitions for all network packet structures and types.
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
    FOG_SNAPSHOT       = 14, /**< Periodic fog-of-war grid bits. */
    RESOURCE_SPAWNED   = 15, /**< A resource node was spawned. */
    RESOURCE_DEPLETED  = 16, /**< A resource node was depleted or destroyed. */
    BUILDING_SPAWNED   = 17, /**< A building was spawned. */
    BUILDING_DESTROYED = 18, /**< A building was destroyed. */
    UPGRADE_COMPLETED  = 19, /**< An upgrade path was completed. */
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
    float      x = 0.f, y = 0.f, z = 0.f;            /**< Initial position. */
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
    float      x = 0.f, y = 0.f, z = 0.f;       /**< Initial position. */
};

/**
 * @struct EntitySnapshotPacket
 * @brief Regular update from server containing entity state.
 */
struct EntitySnapshotPacket {
    PacketType type  = PacketType::ENTITY_SNAPSHOT; /**< Packet type identifier. */
    uint32_t   netId = 0;                            /**< Network ID of the entity. */
    float      x  = 0.f, y  = 0.f, z  = 0.f;         /**< Current position. */
    float      rx = 0.f, ry = 0.f, rz = 0.f;         /**< Current rotation (Euler angles). */
    float      sx = 1.f, sy = 1.f, sz = 1.f;         /**< Current scale. */
    float      vx = 0.f, vy = 0.f, vz = 0.f;         /**< Current velocity. */
};

/**
 * @struct PlayerInputPacket
 * @brief Sent by client to server to communicate player intentions.
 */
struct PlayerInputPacket {
    PacketType type = PacketType::PLAYER_INPUT; /**< Packet type identifier. */
    float      dx   = 0.f;                       /**< Horizontal movement input. */
    float      dy   = 0.f;                       /**< Vertical movement input. */
    float      dz   = 0.f;                       /**< Forward/backward movement input. */
    float      yaw  = 0.f;                       /**< Current camera yaw. */
    float      pitch = 0.f;                      /**< Current camera pitch. */
};

/**
 * @struct UnitSpawnedPacket
 * @brief Sent when a new unit entity is spawned in the world.
 */
struct UnitSpawnedPacket {
    PacketType type    = PacketType::UNIT_SPAWNED;
    uint32_t   netId   = 0;
    uint32_t   teamId  = 0;
    uint8_t    bugClass = 0;
    float      x = 0.f, y = 0.f, z = 0.f;
    float      hp = 100.f, maxHp = 100.f;
};

/**
 * @struct UnitDiedPacket
 * @brief Sent when a unit entity dies.
 */
struct UnitDiedPacket {
    PacketType type  = PacketType::UNIT_DIED;
    uint32_t   netId = 0;
};

/**
 * @struct UnitHpUpdatePacket
 * @brief Periodic health sync for a unit.
 */
struct UnitHpUpdatePacket {
    PacketType type  = PacketType::UNIT_HP_UPDATE;
    uint32_t   netId = 0;
    float      hp    = 0.f;
};

/**
 * @struct GameOverPacket
 * @brief Broadcast when the game ends.
 */
struct GameOverPacket {
    PacketType type        = PacketType::GAME_OVER;
    uint32_t   winnerTeam  = 0xFFFFFFFFu; ///< 0xFFFFFFFF = draw/no winner
};

/**
 * @struct TerritorySnapshotPacket
 * @brief Server → Client: state of all territory zones.
 *
 * Sent periodically (e.g. 2 Hz) so clients can render the territory overlay
 * without needing direct access to the server registry.
 */
struct TerritoryZoneData {
    char     name[32]      = "Zone";
    float    halfW         = 8.f;
    float    halfD         = 8.f;
    float    captureTime   = 10.f;
    float    captureProgress = 0.f;
    uint32_t ownerTeam     = 0xFFFF'FFFFu;
    uint32_t contestedBy   = 0xFFFF'FFFFu;
    float    centerX = 0.f, centerY = 0.f, centerZ = 0.f;
};

struct TerritorySnapshotPacket {
    PacketType type      = PacketType::TERRITORY_SNAPSHOT;
    uint32_t   zoneCount = 0;
    TerritoryZoneData zones[8]{};
};

/**
 * @struct FogSnapshotPacket
 * @brief Server → Client: bit-packed revealed-cell grid for the fog of war.
 *
 * The grid is serialised as an array of uint64_t words.  Bit i of word w
 * corresponds to cell index w*64+i (row-major: z*cellsX + x).
 */
struct FogSnapshotPacket {
    PacketType type    = PacketType::FOG_SNAPSHOT;
    uint16_t   cellsX  = 0;
    uint16_t   cellsZ  = 0;
    /// Enough for a 100×100 grid (10 000 bits → 157 uint64s).
    uint64_t   gridData[160]{};
};

/**
 * @struct ResourceSpawnedPacket
 * @brief Server → Client: a resource node appeared (permanent spawn or meat drop).
 */
struct ResourceSpawnedPacket {
    PacketType type     = PacketType::RESOURCE_SPAWNED;
    uint32_t   netId    = 0;
    uint8_t    resourceType = 1; ///< ResourceType enum value.
    float      x = 0.f, y = 0.f, z = 0.f;
};

/**
 * @struct ResourceDepletedPacket
 * @brief Server → Client: a resource node has been depleted or destroyed.
 */
struct ResourceDepletedPacket {
    PacketType type  = PacketType::RESOURCE_DEPLETED;
    uint32_t   netId = 0;
};

/**
 * @struct CommanderOrderPacket
 * @brief Client → Server: move selected units to a position.
 *
 * Includes the list of selected unit netIds so the server knows
 * exactly which units were selected (selection state is not synced).
 */
struct CommanderOrderPacket {
    PacketType type          = PacketType::COMMANDER_ORDER;
    uint32_t   playerId      = 0;
    float      x = 0.f, y = 0.f, z = 0.f; ///< World destination
    uint32_t   selectedCount = 0;           ///< How many units are selected (max 32).
    uint32_t   selectedNetIds[32]{};        ///< NetIds of selected units.
};

/**
 * @struct BuildingSpawnedPacket
 * @brief Server → Client: a building was constructed.
 */
struct BuildingSpawnedPacket {
    PacketType type    = PacketType::BUILDING_SPAWNED;
    uint32_t   netId   = 0;
    uint32_t   teamId  = 0;
    uint8_t    buildingType = 0; ///< BuildingType enum value.
    uint8_t    specialType  = 0; ///< SpecialBuildingType enum value (for Special buildings).
    uint32_t   tier    = 1;
    float      x = 0.f, y = 0.f, z = 0.f;
    float      hp = 500.f, maxHp = 500.f;
};

/**
 * @struct BuildingDestroyedPacket
 * @brief Server → Client: a building was destroyed.
 */
struct BuildingDestroyedPacket {
    PacketType type  = PacketType::BUILDING_DESTROYED;
    uint32_t   netId = 0;
};

/**
 * @struct UpgradeCompletedPacket
 * @brief Server → Client: a team completed an upgrade path.
 */
struct UpgradeCompletedPacket {
    PacketType type   = PacketType::UPGRADE_COMPLETED;
    uint32_t   pathId = 0;    ///< UpgradePathID
    uint32_t   teamId = 0;    ///< Which team completed it
};
