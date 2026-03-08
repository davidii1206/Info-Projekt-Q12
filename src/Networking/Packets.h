#pragma once
#include <cstdint>

enum class PacketType : uint8_t {
    PING = 1,
};

struct PingPacket {
    PacketType type = PacketType::PING;
    uint32_t timestamp = 0;
};
