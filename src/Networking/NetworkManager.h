/**
 * @file NetworkManager.h
 * @brief High-level networking interface for the engine.
 */

#pragma once
#include "NetworkWrapper.h"
#include "Packets.h"
#include <string>

/**
 * @enum NetworkState
 * @brief Represents the current connectivity state of the engine.
 */
enum class NetworkState {
    Idle,      /**< Not connected and not hosting. */
    Hosting,   /**< Running as an authoritative server. */
    Connected, /**< Connected to a remote server as a client. */
};

/**
 * @class NetworkManager
 * @brief Manages the networking lifecycle and packet communication.
 * 
 * Provides an abstraction over the lower-level NetLib, allowing for 
 * starting hosts, connecting to servers, and sending/receiving packets.
 */
class NetworkManager {
public:
    /**
     * @brief Constructs a new NetworkManager.
     */
    NetworkManager();

    /**
     * @brief Destroys the NetworkManager and cleans up any active session.
     */
    ~NetworkManager();

    /**
     * @brief Initiates hosting a server.
     * @param port The port to host on.
     * @return True if successful.
     */
    bool StartHost(uint16_t port = 25565);

    /**
     * @brief Connects to a remote server.
     * @param host IP address or hostname.
     * @param port Remote port.
     * @return True if successful.
     */
    bool Connect(const std::string& host, uint16_t port = 25565);

    /**
     * @brief Disconnects from the current session.
     */
    void Disconnect();

    /**
     * @brief Processes network events. Should be called every frame.
     */
    void Update();

    /**
     * @brief Gets the current network state.
     * @return Current NetworkState.
     */
    NetworkState GetState() const { return m_State; }

    /**
     * @brief Checks if the engine is currently hosting.
     * @return True if hosting.
     */
    bool IsHosting()   const { return m_State == NetworkState::Hosting; }

    /**
     * @brief Checks if the engine is currently connected (as client or host).
     * @return True if connected.
     */
    bool IsConnected() const { return m_State != NetworkState::Idle; }

    /**
     * @brief Receives one packet sent by the server to this client.
     * 
     * @tparam T The packet structure type.
     * @param type The type tag of the packet.
     * @return The packet if available, otherwise std::nullopt.
     */
    template<typename T>
    std::optional<T> ReceiveFromServer(PacketType type) {
        return m_Session.receiveFromServer<T>(static_cast<uint8_t>(type));
    }

    /**
     * @brief Receives one packet sent by a client to the server.
     * 
     * @tparam T The packet structure type.
     * @param type The type tag of the packet.
     * @return A pair containing the packet and the sender's peerId if available, otherwise std::nullopt.
     */
    template<typename T>
    std::optional<std::pair<T, uint32_t>> ReceiveFromClient(PacketType type) {
        return m_Session.receiveFromClient<T>(static_cast<uint8_t>(type));
    }

    /**
     * @brief Sends a packet from this client to the server.
     * 
     * @tparam T The packet structure type.
     * @param packet The packet data to send.
     * @param reliable Whether the packet must be delivered reliably.
     */
    template<typename T>
    void Send(const T& packet, bool reliable = true) {
        m_Session.send(packet, reliable);
    }

    /**
     * @brief Sends a packet from the server to a specific client.
     * 
     * @tparam T The packet structure type.
     * @param peerId The unique identifier of the destination client.
     * @param packet The packet data to send.
     * @param reliable Whether the packet must be delivered reliably.
     */
    template<typename T>
    void SendToClient(uint32_t peerId, const T& packet, bool reliable = true) {
        m_Session.sendToClient(peerId, packet, reliable);
    }

    /**
     * @brief Broadcasts a packet from the server to all connected clients.
     * 
     * @tparam T The packet structure type.
     * @param packet The packet data to broadcast.
     * @param reliable Whether the packet must be delivered reliably.
     */
    template<typename T>
    void BroadcastToAll(const T& packet, bool reliable = true) {
        m_Session.broadcastToAll(packet, reliable);
    }

    /**
     * @brief Polls connection and disconnection events from the server.
     * 
     * @param peerId Output parameter for the peerId of the player.
     * @param isConnect Output parameter: true for connection, false for disconnection.
     * @return True if an event was polled, false if no events are pending.
     */
    bool PollPlayerConnection(uint32_t& peerId, bool& isConnect) {
        return m_Session.pollPlayerConnection(peerId, isConnect);
    }

private:
    NetLib::GameSession<PacketType> m_Session; /**< Underlying generic network session. */
    NetworkState m_State = NetworkState::Idle; /**< Current high-level network state. */
};
