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

    // Receive one packet sent by the server to this client.
    // Returns nullopt when the queue for that type is empty.
    template<typename T>
    std::optional<T> ReceiveFromServer(PacketType type) {
        return m_Session.receiveFromServer<T>(static_cast<uint8_t>(type));
    }

    // Receive one packet sent by a client to the server.
    // Returns {packet, senderPeerId} or nullopt. Server-only (returns nullopt for clients).
    template<typename T>
    std::optional<std::pair<T, uint32_t>> ReceiveFromClient(PacketType type) {
        return m_Session.receiveFromClient<T>(static_cast<uint8_t>(type));
    }

    // Send a packet from this client to the server.
    template<typename T>
    void Send(const T& packet, bool reliable = true) {
        m_Session.send(packet, reliable);
    }

    // Send a packet from the server to a specific client by peerId. Server-only.
    template<typename T>
    void SendToClient(uint32_t peerId, const T& packet, bool reliable = true) {
        m_Session.sendToClient(peerId, packet, reliable);
    }

    // Broadcast a packet from the server to all connected clients. Server-only.
    template<typename T>
    void BroadcastToAll(const T& packet, bool reliable = true) {
        m_Session.broadcastToAll(packet, reliable);
    }

    // Poll one connection/disconnection event from the server. Server-only.
    bool PollPlayerConnection(uint32_t& peerId, bool& isConnect) {
        return m_Session.pollPlayerConnection(peerId, isConnect);
    }

private:
    NetLib::GameSession<PacketType> m_Session;
    NetworkState m_State = NetworkState::Idle;
};
