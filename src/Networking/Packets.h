/**
 * @file Packets.h
 * @brief Definitions for all network packet structures and types.
 */

#pragma once
#include <cstdint>
#include "../Gameplay/CameraMode.h"

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
    FOG_UPDATE       = 13,/**< Server broadcasts fog-of-war grid to clients. */
    BASE_HP_UPDATE   = 14,/**< Broadcasts updated HP for a base. */
    BUILDING_UPGRADED = 15,/**< Broadcasts a building upgrade completion. */
    BASE_SPAWNED      = 16,/**< Broadcasts that a base entity was created (companion to ASSET_JOINED). */
    BUILDING_PLACED   = 17,/**< Server → Client broadcast: a building was placed in the world. */
    BUILD_PLACE_REQUEST = 18,/**< Client → Server: request to place a building. */

    // Client → Server
    PLAYER_INPUT     = 10, /**< Sends movement and rotation input from client to server. */
    COMMANDER_ORDER  = 12, /**< Client sends move order to server for selected units. */
};

/// Maximum units that can be selected in a single commander order.
constexpr uint32_t MAX_SELECTED_UNITS = 64;

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
    float      hp = -1.f;                              /**< Current HP (-1 = not applicable). */
    float      maxHp = -1.f;                           /**< Max HP (-1 = not applicable). */
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
    CameraMode cameraMode = CameraMode::Commander; /**< Current camera mode. */
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
 * @struct FogUpdatePacket
 * @brief Server → Client: bit-packed fog-of-war grid.
 */
struct FogUpdatePacket {
    PacketType type     = PacketType::FOG_UPDATE;
    int        cellsX   = 0;
    int        cellsZ   = 0;
    uint8_t    data[512]{}; ///< Bit-packed revealed cells (supports up to 4096 cells).
};

/**
 * @struct BuildingUpgradedPacket
 * @brief Server → Client: a building finished an upgrade.
 */
struct BuildingUpgradedPacket {
    PacketType type         = PacketType::BUILDING_UPGRADED;
    uint32_t   netId        = 0;
    uint32_t   newTier      = 1;
    float      newMaxHp     = 100.f;
};

/**
 * @struct BaseSpawnedPacket
 * @brief Server → Client: companion to ASSET_JOINED for base entities.
 *        Tells the client to add BaseHealthComponent to an existing entity.
 */
struct BaseSpawnedPacket {
    PacketType type    = PacketType::BASE_SPAWNED;
    uint32_t   netId   = 0;
    uint32_t   teamId  = 0;
    float      hp      = 500.f;
    float      maxHp   = 500.f;
};

/**
 * @struct BuildPlaceRequestPacket
 * @brief Client → Server: request to place a building.
 */
struct BuildPlaceRequestPacket {
    PacketType type      = PacketType::BUILD_PLACE_REQUEST;
    uint32_t   playerId  = 0;
    uint8_t    buildingType = 0; ///< BuildingType enum value.
    float      x = 0.f, z = 0.f; ///< Ground position (y=0).
};

/**
 * @struct BuildingPlacedPacket
 * @brief Server → Client broadcast: a building was placed.
 */
struct BuildingPlacedPacket {
    PacketType type      = PacketType::BUILDING_PLACED;
    uint32_t   netId     = 0;
    uint32_t   teamId    = 0;
    uint8_t    buildingType = 0;
    uint8_t    bugClass  = 0;
    float      x = 0.f, y = 0.f, z = 0.f;
    float      buildTime = 5.f; ///< Construction duration in seconds.
};

/**
 * @struct CommanderOrderPacket
 * @brief Client → Server: move selected units to a position.
 */
struct CommanderOrderPacket {
    PacketType type     = PacketType::COMMANDER_ORDER;
    uint32_t   playerId = 0;
    float      x = 0.f, y = 0.f, z = 0.f; ///< World destination
    uint32_t   selectedCount = 0;           ///< Number of selected unit netIds.
    uint32_t   selectedIds[MAX_SELECTED_UNITS]{}; ///< NetIds of selected units.
};
