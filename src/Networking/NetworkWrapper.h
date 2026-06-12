/**
 * @file NetworkWrapper.h
 * @brief Low-level network wrapper around ENet.
 */

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
 * @brief Internal networking library components.
 */
namespace NetLib {

/**
 * @class ThreadSafeQueue
 * @brief A simple thread-safe wrapper for std::queue.
 * @tparam T The type of elements held in the queue.
 */
template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue; /**< Underlying queue. */
    std::mutex mutex;    /**< Mutex for thread safety. */
    
public:
    /**
     * @brief Pushes an item onto the queue.
     * @param item The item to push.
     */
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(item);
    }
    
    /**
     * @brief Attempts to pop an item from the queue.
     * @param item Reference to store the popped item.
     * @return True if an item was popped, false if the queue was empty.
     */
    bool pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty()) return false;
        item = queue.front();
        queue.pop();
        return true;
    }
    
    /**
     * @brief Checks if the queue is empty.
     * @return True if empty.
     */
    bool empty() {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.empty();
    }
};

/**
 * @struct Packet
 * @brief Represents a raw network packet with data and sender information.
 */
struct Packet {
    std::vector<uint8_t> data; /**< Raw binary data. */
    uint32_t senderId;         /**< Identifier of the sender peer. */
    
    /**
     * @brief Constructs an empty packet.
     */
    Packet() : senderId(0) {}
    
    /**
     * @brief Constructs a packet from a POD structure.
     * @tparam T The structure type.
     * @param structure The structure instance to copy into the packet.
     */
    template<typename T>
    Packet(const T& structure) : senderId(0) {
        data.resize(sizeof(T));
        memcpy(data.data(), &structure, sizeof(T));
    }
    
    /**
     * @brief Casts the internal data buffer to a pointer of type T.
     * @tparam T The structure type to cast to.
     * @return A pointer to the data as T, or nullptr if the buffer is too small.
     */
    template<typename T>
    T* as() {
        if (data.size() < sizeof(T)) return nullptr;
        return reinterpret_cast<T*>(data.data());
    }
};

/**
 * @class Client
 * @brief Low-level ENet client wrapper.
 */
class Client {
private:
    ENetHost* client;           /**< ENet client host. */
    ENetPeer* serverPeer;       /**< Peer representing the server. */
    std::thread networkThread;  /**< Background thread for network servicing. */
    bool running;               /**< Flag to control the background thread. */
    ThreadSafeQueue<Packet> incomingPackets; /**< Queue for received packets. */
    
public:
    /**
     * @brief Constructs an uninitialized client.
     */
    Client() : client(nullptr), serverPeer(nullptr), running(false) {}

    /**
     * @brief Destructor ensures proper disconnection and cleanup.
     */
    ~Client() { disconnect(); }
    
    /**
     * @brief Connects to a server.
     * @param host Hostname or IP address.
     * @param port Remote port.
     * @param timeoutMs Connection timeout in milliseconds.
     * @return True if connected successfully.
     */
    bool connect(const char* host, uint16_t port, uint32_t timeoutMs = 5000) {
        client = enet_host_create(nullptr, 1, 2, 0, 0);
        if (!client) return false;
        
        ENetAddress address;
        memset(&address, 0, sizeof(ENetAddress));
        
        if (enet_address_set_host(&address, host) != 0) {
            enet_host_destroy(client);
            client = nullptr;
            return false;
        }
        address.port = port;
        
        serverPeer = enet_host_connect(client, &address, 2, 0);
        if (!serverPeer) {
            enet_host_destroy(client);
            client = nullptr;
            return false;
        }
        
        ENetEvent event;
        if (enet_host_service(client, &event, timeoutMs) > 0 && 
            event.type == ENET_EVENT_TYPE_CONNECT) {
            running = true;
            networkThread = std::thread(&Client::networkLoop, this);
            return true;
        }
        
        enet_peer_reset(serverPeer);
        enet_host_destroy(client);
        client = nullptr;
        serverPeer = nullptr;
        return false;
    }
    
    /**
     * @brief Gracefully disconnects from the server.
     */
    void disconnect() {
        running = false;
        if (serverPeer) {
            enet_peer_disconnect(serverPeer, 0);
            ENetEvent event;
            while (enet_host_service(client, &event, 500) > 0) {
                if (event.type == ENET_EVENT_TYPE_DISCONNECT) break;
            }
        }
        if (networkThread.joinable()) networkThread.join();
        if (client) {
            enet_host_destroy(client);
            client = nullptr;
        }
        serverPeer = nullptr;
    }
    
    /**
     * @brief Sends a packet to the server.
     * @tparam T The structure type.
     * @param structure The packet data.
     * @param reliable Whether the packet must be delivered reliably.
     */
    template<typename T>
    void send(const T& structure, bool reliable = true) {
        if (!serverPeer || !running) return;
        ENetPacket* packet = enet_packet_create(&structure, sizeof(T),
            reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
        enet_peer_send(serverPeer, 0, packet);
    }
    
    /**
     * @brief Pops one packet from the incoming queue.
     * @param packet Reference to store the popped packet.
     * @return True if a packet was received.
     */
    bool receive(Packet& packet) {
        return incomingPackets.pop(packet);
    }
    
    /**
     * @brief Checks if the client is currently connected.
     * @return True if connected and running.
     */
    bool isConnected() const { return serverPeer != nullptr && running; }
    
private:
    /**
     * @brief Main loop for the background network thread.
     */
    void networkLoop() {
        ENetEvent event;
        while (running) {
            while (enet_host_service(client, &event, 10) > 0) {
                if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                    Packet pkt;
                    pkt.data.assign(event.packet->data, 
                                   event.packet->data + event.packet->dataLength);
                    incomingPackets.push(pkt);
                    enet_packet_destroy(event.packet);
                } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                    running = false;
                    serverPeer = nullptr;
                }
            }
        }
    }
};

/**
 * @class Server
 * @brief Low-level ENet server wrapper.
 */
class Server {
private:
    ENetHost* server;           /**< ENet server host. */
    std::thread networkThread;  /**< Background thread for network servicing. */
    bool running;               /**< Flag to control the background thread. */
    ThreadSafeQueue<Packet> incomingPackets; /**< Queue for received packets. */
    std::mutex peersMutex;      /**< Mutex protecting the peers list. */
    std::vector<ENetPeer*> peers; /**< List of currently connected client peers. */
    
    /**
     * @struct ConnectionEvent
     * @brief Internal tracking for connect/disconnect events.
     */
    struct ConnectionEvent {
        uint32_t peerId;
        bool isConnect;
    };
    ThreadSafeQueue<ConnectionEvent> connectionEvents; /**< Queue for connectivity events. */
    
public:
    /**
     * @brief Constructs an uninitialized server.
     */
    Server() : server(nullptr), running(false) {}

    /**
     * @brief Destructor ensures proper shutdown.
     */
    ~Server() { stop(); }
    
    /**
     * @brief Starts the server.
     * @param port Port to listen on.
     * @param maxClients Maximum allowed simultaneous connections.
     * @return True if the server started successfully.
     */
    bool start(uint16_t port, uint32_t maxClients = 32) {
        ENetAddress address;
        memset(&address, 0, sizeof(ENetAddress));
        address.host = ENET_HOST_ANY;
        address.port = port;
        
        server = enet_host_create(&address, maxClients, 2, 0, 0);
        if (!server) return false;
        
        running = true;
        networkThread = std::thread(&Server::networkLoop, this);
        return true;
    }
    
    /**
     * @brief Stops the server and cleans up resources.
     */
    void stop() {
        running = false;
        if (networkThread.joinable()) networkThread.join();
        if (server) {
            enet_host_destroy(server);
            server = nullptr;
        }
    }
    
    /**
     * @brief Broadcasts a packet to all connected clients.
     * @tparam T The structure type.
     * @param structure The packet data.
     * @param reliable Whether the packet must be delivered reliably.
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
     * @brief Sends a packet to a specific client.
     * @tparam T The structure type.
     * @param peerId The ID of the destination client.
     * @param structure The packet data.
     * @param reliable Whether the packet must be delivered reliably.
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
    
    /**
     * @brief Pops one packet from the incoming queue.
     * @param packet Reference to store the popped packet.
     * @return True if a packet was received.
     */
    bool receive(Packet& packet) {
        return incomingPackets.pop(packet);
    }
    
    /**
     * @brief Polls the next connection or disconnection event.
     * @param peerId Output peer identifier.
     * @param isConnect Output flag: true if connect, false if disconnect.
     * @return True if an event was popped.
     */
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
    /**
     * @brief Main loop for the background network thread.
     */
    void networkLoop() {
        ENetEvent event;
        while (running) {
            while (enet_host_service(server, &event, 10) > 0) {
                switch (event.type) {
                    case ENET_EVENT_TYPE_CONNECT: {
                        std::lock_guard<std::mutex> lock(peersMutex);
                        size_t peerId = peers.size();
                        event.peer->data = (void*)peerId;
                        peers.push_back(event.peer);
                        
                        ConnectionEvent connEvent;
                        connEvent.peerId = (uint32_t)peerId;
                        connEvent.isConnect = true;
                        connectionEvents.push(connEvent);
                        break;
                    }
                    case ENET_EVENT_TYPE_RECEIVE: {
                        Packet pkt;
                        pkt.senderId = (uint32_t)(size_t)event.peer->data;
                        pkt.data.assign(event.packet->data, 
                                       event.packet->data + event.packet->dataLength);
                        incomingPackets.push(pkt);
                        enet_packet_destroy(event.packet);
                        break;
                    }
                    case ENET_EVENT_TYPE_DISCONNECT: {
                        std::lock_guard<std::mutex> lock(peersMutex);
                        size_t peerId = (size_t)event.peer->data;
                        if (peerId < peers.size()) peers[peerId] = nullptr;
                        
                        ConnectionEvent connEvent;
                        connEvent.peerId = (uint32_t)peerId;
                        connEvent.isConnect = false;
                        connectionEvents.push(connEvent);
                        break;
                    }
                }
            }
        }
    }
};

/**
 * @class GameSession
 * @brief High-level session management combining server and client functionality.
 * @tparam PacketHeaderType The enum or type used for the first byte of packets to identify their type.
 */
template<typename PacketHeaderType>
class GameSession {
private:
    Server* server;             /**< Pointer to the server instance (if hosting). */
    Client* client;             /**< Pointer to the client instance. */
    bool isHost;                /**< True if this session is hosting the server. */
    uint32_t myPlayerId;        /**< Assigned player ID from the server. */
    bool idAssigned;            /**< Flag indicating if myPlayerId has been set. */
    
    std::map<uint8_t, std::queue<Packet>> clientPacketQueues; /**< Incoming packets from server sorted by type. */
    std::map<uint8_t, std::queue<Packet>> serverPacketQueues; /**< Incoming packets from clients sorted by type. */
    std::mutex clientQueueMutex; /**< Mutex for clientPacketQueues. */
    std::mutex serverQueueMutex; /**< Mutex for serverPacketQueues. */
    
    /**
     * @struct ConnectionEvent
     * @brief Tracker for connectivity changes.
     */
    struct ConnectionEvent {
        uint32_t peerId;
        bool isConnect;
    };
    std::queue<ConnectionEvent> connectionQueue; /**< Internal queue for connection events. */
    std::mutex connectionMutex;                  /**< Mutex for connectionQueue. */
    
public:
    /**
     * @brief Constructs an idle GameSession.
     */
    GameSession() : server(nullptr), client(nullptr), isHost(false), 
                    myPlayerId(999), idAssigned(false) {}
    
    /**
     * @brief Destructor cleans up all network resources.
     */
    ~GameSession() {
        cleanup();
    }
    
    /**
     * @brief Starts a host session (server + local client).
     * @param port Port to listen on.
     * @param maxClients Max clients allowed.
     * @return True if both server and client started successfully.
     */
    bool startHost(uint16_t port = 25565, uint32_t maxClients = 32) {
        if (enet_initialize() != 0) return false;
        
        isHost = true;
        server = new Server();
        if (!server->start(port, maxClients)) {
            delete server;
            server = nullptr;
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        client = new Client();
        if (!client->connect("127.0.0.1", port)) {
            delete client;
            delete server;
            client = nullptr;
            server = nullptr;
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Connects to a server as a client only.
     * @param host Server IP or hostname.
     * @param port Server port.
     * @return True if connection process started successfully.
     */
    bool connectToServer(const char* host, uint16_t port = 25565) {
        if (enet_initialize() != 0) return false;
        
        isHost = false;
        client = new Client();
        if (!client->connect(host, port)) {
            delete client;
            client = nullptr;
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Pumps packets from the low-level Client/Server into the typed GameSession queues.
     * 
     * Should be called frequently (e.g., once per frame).
     */
    void update() {
        if (server) {
            Packet pkt;
            while (server->receive(pkt)) {
                if (pkt.data.size() >= sizeof(PacketHeaderType)) {
                    PacketHeaderType* header = pkt.as<PacketHeaderType>();
                    uint8_t type = *reinterpret_cast<uint8_t*>(header);
                    
                    std::lock_guard<std::mutex> lock(serverQueueMutex);
                    serverPacketQueues[type].push(pkt);
                }
            }
            
            uint32_t peerId;
            bool isConnect;
            while (server->pollConnection(peerId, isConnect)) {
                std::lock_guard<std::mutex> lock(connectionMutex);
                connectionQueue.push({peerId, isConnect});
            }
        }
        
        if (client) {
            Packet pkt;
            while (client->receive(pkt)) {
                if (pkt.data.size() >= sizeof(PacketHeaderType)) {
                    PacketHeaderType* header = pkt.as<PacketHeaderType>();
                    uint8_t type = *reinterpret_cast<uint8_t*>(header);
                    
                    std::lock_guard<std::mutex> lock(clientQueueMutex);
                    clientPacketQueues[type].push(pkt);
                }
            }
        }
    }
    
    /**
     * @brief Receives one packet from the server.
     * @tparam T The packet structure.
     * @param packetType The identifier for the packet type.
     * @return The packet if available.
     */
    template<typename T>
    std::optional<T> receiveFromServer(uint8_t packetType) {
        std::lock_guard<std::mutex> lock(clientQueueMutex);
        
        auto& queue = clientPacketQueues[packetType];
        if (queue.empty()) return std::nullopt;
        
        Packet pkt = queue.front();
        queue.pop();
        
        T* data = pkt.as<T>();
        if (data) return *data;
        return std::nullopt;
    }
    
    /**
     * @brief Receives one packet from the server, including sender info.
     * @tparam T The packet structure.
     * @param packetType The identifier for the packet type.
     * @return A pair of (packet, senderId) if available.
     */
    template<typename T>
    std::optional<std::pair<T, uint32_t>> receiveFromServerWithSender(uint8_t packetType) {
        std::lock_guard<std::mutex> lock(clientQueueMutex);
        
        auto& queue = clientPacketQueues[packetType];
        if (queue.empty()) return std::nullopt;
        
        Packet pkt = queue.front();
        queue.pop();
        
        T* data = pkt.as<T>();
        if (data) return std::make_pair(*data, pkt.senderId);
        return std::nullopt;
    }
    
    /**
     * @brief Receives one packet sent by a client (server-side).
     * @tparam T The packet structure.
     * @param packetType The identifier for the packet type.
     * @return A pair of (packet, senderId) if available.
     */
    template<typename T>
    std::optional<std::pair<T, uint32_t>> receiveFromClient(uint8_t packetType) {
        if (!isHost) return std::nullopt;
        
        std::lock_guard<std::mutex> lock(serverQueueMutex);
        
        auto& queue = serverPacketQueues[packetType];
        if (queue.empty()) return std::nullopt;
        
        Packet pkt = queue.front();
        queue.pop();
        
        T* data = pkt.as<T>();
        if (data) return std::make_pair(*data, pkt.senderId);
        return std::nullopt;
    }
    
    /**
     * @brief Polls connection events from the server.
     * @param peerId Output peer identifier.
     * @param isConnect Output flag (true=connect).
     * @return True if an event was polled.
     */
    bool pollPlayerConnection(uint32_t& peerId, bool& isConnect) {
        if (!isHost) return false;
        
        std::lock_guard<std::mutex> lock(connectionMutex);
        if (connectionQueue.empty()) return false;
        
        auto event = connectionQueue.front();
        connectionQueue.pop();
        
        peerId = event.peerId;
        isConnect = event.isConnect;
        return true;
    }
    
    /**
     * @brief Sends a packet to the server.
     * @tparam T The structure type.
     * @param packet The packet data.
     * @param reliable Whether the packet must be delivered reliably.
     */
    template<typename T>
    void send(const T& packet, bool reliable = true) {
        if (client && client->isConnected()) {
            client->send(packet, reliable);
        }
    }
    
    /**
     * @brief Sends a packet to a specific client (server-side).
     * @tparam T The structure type.
     * @param clientId Destination peer ID.
     * @param packet The packet data.
     * @param reliable Whether the packet must be delivered reliably.
     */
    template<typename T>
    void sendToClient(uint32_t clientId, const T& packet, bool reliable = true) {
        if (server) {
            server->sendTo(clientId, packet, reliable);
        }
    }
    
    /**
     * @brief Broadcasts a packet to all clients (server-side).
     * @tparam T The structure type.
     * @param packet The packet data.
     * @param reliable Whether the packet must be delivered reliably.
     */
    template<typename T>
    void broadcastToAll(const T& packet, bool reliable = true) {
        if (server) {
            server->broadcast(packet, reliable);
        }
    }
    
    /**
     * @brief Gets my assigned player ID.
     * @return The player ID.
     */
    uint32_t getMyPlayerId() const { return myPlayerId; }

    /**
     * @brief Checks if a player ID has been assigned yet.
     * @return True if assigned.
     */
    bool hasPlayerId() const { return idAssigned; }

    /**
     * @brief Checks if this session is the host.
     * @return True if hosting.
     */
    bool isHosting() const { return isHost; }
    
    /**
     * @brief Sets my local player ID (typically called when receiving an assign packet).
     * @param id The assigned ID.
     */
    void assignMyPlayerId(uint32_t id) {
        if (!idAssigned) {
            myPlayerId = id;
            idAssigned = true;
        }
    }
    
    /**
     * @brief Stops all network activity and frees resources.
     */
    void cleanup() {
        if (client) {
            delete client;
            client = nullptr;
        }
        if (server) {
            delete server;
            server = nullptr;
        }
        enet_deinitialize();
    }
};

} // namespace NetLib
