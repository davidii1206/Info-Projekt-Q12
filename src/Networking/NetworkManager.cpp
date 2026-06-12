/**
 * @file NetworkManager.cpp
 * @brief Implementation of the NetworkManager class.
 */

#include "NetworkManager.h"
#include <spdlog/spdlog.h>

/**
 * @brief Default constructor for NetworkManager.
 */
NetworkManager::NetworkManager() = default;

/**
 * @brief Destructor for NetworkManager.
 * 
 * Ensures all network sessions are properly closed and resources released.
 */
NetworkManager::~NetworkManager() {
    Disconnect();
}

/**
 * @brief Starts hosting a server on the specified port.
 * @param port The port to listen for incoming client connections.
 * @return True if the host was successfully started, false otherwise.
 */
bool NetworkManager::StartHost(uint16_t port) {
    if (m_State != NetworkState::Idle) return false;

    if (!m_Session.startHost(port)) {
        spdlog::error("NetworkManager: failed to start host on port {}", port);
        return false;
    }

    m_State = NetworkState::Hosting;
    spdlog::info("NetworkManager: hosting on port {}", port);
    return true;
}

/**
 * @brief Attempts to connect to a remote server.
 * @param host The IP address or hostname of the server.
 * @param port The port the server is listening on.
 * @return True if the connection process was successfully initiated, false otherwise.
 */
bool NetworkManager::Connect(const std::string& host, uint16_t port) {
    if (m_State != NetworkState::Idle) return false;

    if (!m_Session.connectToServer(host.c_str(), port)) {
        spdlog::error("NetworkManager: failed to connect to {}:{}", host, port);
        return false;
    }

    m_State = NetworkState::Connected;
    spdlog::info("NetworkManager: connected to {}:{}", host, port);
    return true;
}

/**
 * @brief Gracefully disconnects from the current network session.
 * 
 * Resets the network state to Idle and cleans up session resources.
 */
void NetworkManager::Disconnect() {
    if (m_State == NetworkState::Idle) return;

    m_Session.cleanup();
    m_State = NetworkState::Idle;
    spdlog::info("NetworkManager: disconnected");
}

/**
 * @brief Processes incoming network events and maintains session state.
 * 
 * Should be called once per frame.
 */
void NetworkManager::Update() {
    if (m_State == NetworkState::Idle) return;

    m_Session.update();
}
