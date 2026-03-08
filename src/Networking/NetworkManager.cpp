#include "NetworkManager.h"
#include <spdlog/spdlog.h>

NetworkManager::NetworkManager() = default;

NetworkManager::~NetworkManager() {
    Disconnect();
}

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

void NetworkManager::Disconnect() {
    if (m_State == NetworkState::Idle) return;

    m_Session.cleanup();
    m_State = NetworkState::Idle;
    spdlog::info("NetworkManager: disconnected");
}

void NetworkManager::Update() {
    if (m_State == NetworkState::Idle) return;

    m_Session.update();
}
