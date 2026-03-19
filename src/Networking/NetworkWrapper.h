#pragma once
#include <enet/enet.h>
#include <thread>
#include <mutex>
#include <queue>
#include <vector>
#include <functional>
#include <map>
#include <cstring>
#include <spdlog/spdlog.h>
#include <optional>

/**
 * @namespace NetLib
 * @brief Namespace containing all networking-related classes and utilities.
 */
namespace NetLib {

/**
 * @class ThreadSafeQueue
 * @brief A simple thread-safe wrapper around std::queue.
 * @tparam T The type of elements stored in the queue.
 */
template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue;
    std::mutex mutex;
    
public:
    /** @brief Pushes an item into the queue. */
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(item);
    }
    
    /** 
     * @brief Attempts to pop an item from the queue. 
     * @param item Reference to store the popped item.
     * @return true if an item was popped, false if the queue was empty.
     */
    bool pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty()) return false;
        item = queue.front();
        queue.pop();
        return true;
    }
    
    /** @brief Checks if the queue is empty. */
    bool empty() {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.empty();
    }
};

/**
 * @struct Packet
 * @brief Represents a raw network packet.
 * 
 * Contains raw binary data and the ID of the sender. 
 * Supports conversion to/from POD structures.
 */
struct Packet {
    std::vector<uint8_t> data; ///< Raw binary data.
    uint32_t senderId;         ///< ID assigned by ENet to the sender.
    
    Packet() : senderId(0) {}
    
    /**
     * @brief Creates a packet from a POD structure.
     * @tparam T The type of the structure.
     * @param structure The structure to copy into the packet.
     */
    template<typename T>
    Packet(const T& structure) : senderId(0) {
        data.resize(sizeof(T));
        memcpy(data.data(), &structure, sizeof(T));
    }
    
    /**
     * @brief Casts the packet data to a specific structure pointer.
     * @tparam T The target structure type.
     * @return Pointer to the data, or nullptr if size is insufficient.
     */
    template<typename T>
    T* as() {
        if (data.size() < sizeof(T)) return nullptr;
        return reinterpret_cast<T*>(data.data());
    }
};

/**
 * @class Client
 * @brief Handles client-side ENet connections and communication.
 * 
 * Runs a dedicated background thread for processing network events.
 */
class Client {
private:
    ENetHost* client;
    ENetPeer* serverPeer;
    std::thread networkThread;
    bool running;
    ThreadSafeQueue<Packet> incomingPackets;
    
public:
    Client() : client(nullptr), serverPeer(nullptr), running(false) {}
    ~Client() { disconnect(); }
    
    /**
     * @brief Connects to a remote server.
     * @param host The hostname or IP address.
     * @param port The port number.
     * @param timeoutMs Connection timeout in milliseconds.
     * @return true if connection was established.
     */
    bool connect(const char* host, uint16_t port, uint32_t timeoutMs = 5000);
    
    /** @brief Disconnects from the server and stops the network thread. */
    void disconnect();
    
    /**
     * @brief Sends a structure to the server.
     * @tparam T Type of the structure.
     * @param structure The data to send.
     * @param reliable Whether to use ENet's reliable delivery.
     */
    template<typename T>
    void send(const T& structure, bool reliable = true) {
        if (!serverPeer || !running) return;
        ENetPacket* packet = enet_packet_create(&structure, sizeof(T),
            reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
        enet_peer_send(serverPeer, 0, packet);
    }
    
    /** @brief Receives the next available packet from the queue. */
    bool receive(Packet& packet) {
        return incomingPackets.pop(packet);
    }
    
    /** @brief Checks if the client is currently connected. */
    bool isConnected() const { return serverPeer != nullptr && running; }
    
private:
    void networkLoop();
};

/**
 * @class Server
 * @brief Handles server-side ENet hosting and peer management.
 * 
 * Manages multiple client connections and provides broadcasting capabilities.
 */
class Server {
private:
    ENetHost* server;
    std::thread networkThread;
    bool running;
    ThreadSafeQueue<Packet> incomingPackets;
    std::mutex peersMutex;
    std::vector<ENetPeer*> peers;
    
    struct ConnectionEvent {
        uint32_t peerId;
        bool isConnect;
    };
    ThreadSafeQueue<ConnectionEvent> connectionEvents;
    
public:
    Server() : server(nullptr), running(false) {}
    ~Server() { stop(); }
    
    /**
     * @brief Starts hosting a server.
     * @param port The port to listen on.
     * @param maxClients Maximum number of concurrent connections.
     * @return true if server started successfully.
     */
    bool start(uint16_t port, uint32_t maxClients = 32);
    
    /** @brief Stops the server and disconnects all clients. */
    void stop();
    
    /**
     * @brief Sends a structure to all connected clients.
     */
    template<typename T>
    void broadcast(const T& structure, bool reliable = true) {
        std::lock_guard<std::mutex> lock(peersMutex);
        ENetPacket* packet = enet_packet_create(&structure, sizeof(T),
            reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
        for (auto* peer : peers) {
            if (peer) enet_peer_send(peer, 0, packet);
        }
    }
    
    /**
     * @brief Sends a structure to a specific client.
     * @param peerId The ID of the target peer.
     */
    template<typename T>
    void sendTo(uint32_t peerId, const T& structure, bool reliable = true) {
        std::lock_guard<std::mutex> lock(peersMutex);
        if (peerId < peers.size() && peers[peerId]) {
            ENetPacket* packet = enet_packet_create(&structure, sizeof(T),
                reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
            enet_peer_send(peers[peerId], 0, packet);
        }
    }
    
    /** @brief Receives the next available packet from the queue. */
    bool receive(Packet& packet) {
        return incomingPackets.pop(packet);
    }
    
    /** @brief Checks for new connections or disconnections. */
    bool pollConnection(uint32_t& peerId, bool& isConnect) {
        ConnectionEvent event;
        if (connectionEvents.pop(event)) {
            peerId = event.peerId;
            isConnect = event.isConnect;
            return true;
        }
        return false;
    }
    
private:
    void networkLoop();
};

/**
 * @class GameSession
 * @brief High-level manager for a multiplayer game session.
 * 
 * Abstracts whether the user is a host or a client and provides
 * type-based packet routing.
 * 
 * @tparam PacketHeaderType The structure type used as a header for all packets.
 */
template<typename PacketHeaderType>
class GameSession {
private:
    Server* server;
    Client* client;
    bool isHost;
    uint32_t myPlayerId;
    bool idAssigned;
    
    std::map<uint8_t, std::queue<Packet>> clientPacketQueues;
    std::map<uint8_t, std::queue<Packet>> serverPacketQueues;
    std::mutex clientQueueMutex;
    std::mutex serverQueueMutex;
    
    struct ConnectionEvent {
        uint32_t peerId;
        bool isConnect;
    };
    std::queue<ConnectionEvent> connectionQueue;
    std::mutex connectionMutex;
    
public:
    GameSession() : server(nullptr), client(nullptr), isHost(false), 
                    myPlayerId(999), idAssigned(false) {}
    
    ~GameSession() { cleanup(); }
    
    /** @brief Starts a local server and connects a local client to it. */
    bool startHost(uint16_t port = 25565, uint32_t maxClients = 32);
    
    /** @brief Connects to a remote game host. */
    bool connectToServer(const char* host, uint16_t port = 25565);
    
    /**
     * @brief Polling function to sort incoming packets into typed queues.
     * Should be called every frame.
     */
    void update();
    
    /**
     * @brief Retrieves the next packet of a specific type from the server.
     * @tparam T The expected structure type.
     * @param packetType The identifier for the packet type.
     */
    template<typename T>
    std::optional<T> receiveFromServer(uint8_t packetType);
    
    /** @brief Polls for player connection events (Server only). */
    bool pollPlayerConnection(uint32_t& peerId, bool& isConnect);
    
    /** @brief Sends a packet to the server. */
    template<typename T>
    void send(const T& packet, bool reliable = true);
    
    /** @brief Broadcasts a packet to all connected clients (Server only). */
    template<typename T>
    void broadcastToAll(const T& packet, bool reliable = true);
    
    /** @brief Gets the local player's network ID. */
    uint32_t getMyPlayerId() const { return myPlayerId; }
    
    /** @brief Checks if this instance is hosting the server. */
    bool isHosting() const { return isHost; }
    
    /** @brief Shuts down networking and releases resources. */
    void cleanup();
};

} // namespace NetLib
