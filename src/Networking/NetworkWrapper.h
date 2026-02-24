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

namespace NetLib {

template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue;
    std::mutex mutex;
    
public:
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(item);
    }
    
    bool pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty()) return false;
        item = queue.front();
        queue.pop();
        return true;
    }
    
    bool empty() {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.empty();
    }
};

struct Packet {
    std::vector<uint8_t> data;
    uint32_t senderId;
    
    Packet() : senderId(0) {}
    
    template<typename T>
    Packet(const T& structure) : senderId(0) {
        data.resize(sizeof(T));
        memcpy(data.data(), &structure, sizeof(T));
    }
    
    template<typename T>
    T* as() {
        if (data.size() < sizeof(T)) return nullptr;
        return reinterpret_cast<T*>(data.data());
    }
};

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
    
    void disconnect() {
        running = false;
        if (serverPeer) {
            enet_peer_disconnect(serverPeer, 0);
            ENetEvent event;
            while (enet_host_service(client, &event, 3000) > 0) {
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
    
    template<typename T>
    void send(const T& structure, bool reliable = true) {
        if (!serverPeer || !running) return;
        ENetPacket* packet = enet_packet_create(&structure, sizeof(T),
            reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
        enet_peer_send(serverPeer, 0, packet);
    }
    
    bool receive(Packet& packet) {
        return incomingPackets.pop(packet);
    }
    
    bool isConnected() const { return serverPeer != nullptr && running; }
    
private:
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
    
    void stop() {
        running = false;
        if (networkThread.joinable()) networkThread.join();
        if (server) {
            enet_host_destroy(server);
            server = nullptr;
        }
    }
    
    template<typename T>
    void broadcast(const T& structure, bool reliable = true) {
        std::lock_guard<std::mutex> lock(peersMutex);
        ENetPacket* packet = enet_packet_create(&structure, sizeof(T),
            reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
        for (auto* peer : peers) {
            if (peer) enet_peer_send(peer, 0, packet);
        }
    }
    
    template<typename T>
    void sendTo(uint32_t peerId, const T& structure, bool reliable = true) {
        std::lock_guard<std::mutex> lock(peersMutex);
        if (peerId < peers.size() && peers[peerId]) {
            ENetPacket* packet = enet_packet_create(&structure, sizeof(T),
                reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
            enet_peer_send(peers[peerId], 0, packet);
        }
    }
    
    bool receive(Packet& packet) {
        return incomingPackets.pop(packet);
    }
    
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
    
    ~GameSession() {
        cleanup();
    }
    
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
    
    template<typename T>
    void send(const T& packet, bool reliable = true) {
        if (client && client->isConnected()) {
            client->send(packet, reliable);
        }
    }
    
    template<typename T>
    void sendToClient(uint32_t clientId, const T& packet, bool reliable = true) {
        if (server) {
            server->sendTo(clientId, packet, reliable);
        }
    }
    
    template<typename T>
    void broadcastToAll(const T& packet, bool reliable = true) {
        if (server) {
            server->broadcast(packet, reliable);
        }
    }
    
    uint32_t getMyPlayerId() const { return myPlayerId; }
    bool hasPlayerId() const { return idAssigned; }
    bool isHosting() const { return isHost; }
    
    void assignMyPlayerId(uint32_t id) {
        if (!idAssigned) {
            myPlayerId = id;
            idAssigned = true;
        }
    }
    
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

}