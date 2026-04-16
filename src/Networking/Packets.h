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

    // Client → Server
    PLAYER_INPUT = 10,    /**< Sends movement and rotation input from client to server. */
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
