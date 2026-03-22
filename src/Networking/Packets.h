#pragma once
#include <cstdint>

enum class PacketType : uint8_t {
    PING = 1,

    // Server → Client
    PLAYER_ID_ASSIGN = 2, // "you are playerId X, your entity is netId Y"
    PLAYER_JOINED    = 3, // "new player entity: netId Y, playerId X, at position P"
    PLAYER_LEFT      = 4, // "entity with netId Y was removed"
    ENTITY_SNAPSHOT  = 5, // "entity netId Y is now at position P with velocity V"
    ASSET_JOINED     = 6, // "new asset entity: netId Y, modelPath P"

    // Client → Server
    PLAYER_INPUT = 10, // "my movement input is (dx, dz)"
};

struct PingPacket {
    PacketType type      = PacketType::PING;
    uint32_t   timestamp = 0;
};

struct PlayerIdAssignPacket {
    PacketType type     = PacketType::PLAYER_ID_ASSIGN;
    uint32_t   playerId = 0;
    uint32_t   netId    = 0;
};

struct PlayerJoinedPacket {
    PacketType type     = PacketType::PLAYER_JOINED;
    uint32_t   netId    = 0;
    uint32_t   playerId = 0;
    float      x = 0.f, y = 0.f, z = 0.f;
};

struct PlayerLeftPacket {
    PacketType type  = PacketType::PLAYER_LEFT;
    uint32_t   netId = 0;
};

struct AssetJoinedPacket {
    PacketType type = PacketType::ASSET_JOINED;
    uint32_t   netId = 0;
    char       modelPath[128]{};
    float      x = 0.f, y = 0.f, z = 0.f;
};

struct EntitySnapshotPacket {
    PacketType type  = PacketType::ENTITY_SNAPSHOT;
    uint32_t   netId = 0;
    float      x  = 0.f, y  = 0.f, z  = 0.f;
    float      vx = 0.f, vy = 0.f, vz = 0.f;
};

struct PlayerInputPacket {
    PacketType type = PacketType::PLAYER_INPUT;
    float      dx   = 0.f; // normalized XZ input direction
    float      dz   = 0.f;
};
