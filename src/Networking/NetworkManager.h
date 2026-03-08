#pragma once
#include "NetworkWrapper.h"
#include "Packets.h"
#include <string>

enum class NetworkState {
    Idle,
    Hosting,
    Connected,
};

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    bool StartHost(uint16_t port = 25565);
    bool Connect(const std::string& host, uint16_t port = 25565);
    void Disconnect();

    void Update();

    NetworkState GetState() const { return m_State; }
    bool IsHosting()   const { return m_State == NetworkState::Hosting; }
    bool IsConnected() const { return m_State != NetworkState::Idle; }

private:
    NetLib::GameSession<PacketType> m_Session;
    NetworkState m_State = NetworkState::Idle;
};
