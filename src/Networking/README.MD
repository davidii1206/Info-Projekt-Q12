# NetLib - Simple C++ Networking Library

A lightweight, easy-to-use networking library built on top of ENet for game development. Designed for minimal boilerplate with a clean polling-based API.

## Features

- ✅ **Simple polling API** - No callbacks, no lambdas everywhere
- ✅ **Type-safe packet handling** - Send/receive strongly-typed structs
- ✅ **Host and Client modes** - Built-in support for peer-to-peer hosting
- ✅ **Automatic connection management** - Built-in connection/disconnection events
- ✅ **Thread-safe** - Internal threading handled for you
- ✅ **Reliable and unreliable messaging** - Choose per-packet
- ✅ **No global state** - Multiple sessions supported

## Table of Contents

1. [Quick Start](#quick-start)
2. [Core Concepts](#core-concepts)
3. [Packet Definitions](#packet-definitions)
4. [Client-Side Usage](#client-side-usage)
5. [Server-Side Usage](#server-side-usage)
6. [The GameSession API](#the-gamesession-api)
7. [Complete Example](#complete-example)
8. [Best Practices](#best-practices)
9. [API Reference](#api-reference)

---

## Quick Start

### 1. Define Your Packets

```cpp
#include "NetworkWrapper.h"

enum class PacketType : uint8_t {
    CHAT_MESSAGE = 1,
    PLAYER_MOVE = 2,
    PLAYER_SPAWN = 3
};

struct ChatMessagePacket {
    PacketType type = PacketType::CHAT_MESSAGE;  // MUST be first!
    char message[256];
};

struct PlayerMovePacket {
    PacketType type = PacketType::PLAYER_MOVE;
    float x, y, z;
};
```

**Critical Rule:** The first member of every packet **MUST** be the packet type (`uint8_t` or enum based on `uint8_t`).

### 2. Create a Session

```cpp
NetLib::GameSession<PacketType> net;

// Start as host (server + local client)
if (net.startHost(25565)) {
    std::cout << "Hosting on port 25565" << std::endl;
}

// OR connect as client
if (net.connectToServer("192.168.1.100", 25565)) {
    std::cout << "Connected!" << std::endl;
}
```

### 3. Poll and Process Packets

```cpp
void update() {
    net.update();  // MUST call every frame
    
    // Receive from server (client-side)
    while (auto pkt = net.receiveFromServer<ChatMessagePacket>(static_cast<uint8_t>(PacketType::CHAT_MESSAGE))) {
        std::cout << "Chat: " << pkt->message << std::endl;
    }
    
    // Receive from clients (server-side, only if hosting)
    while (auto result = net.receiveFromClient<PlayerMovePacket>(static_cast<uint8_t>(PacketType::PLAYER_MOVE))) {
        auto [pkt, clientId] = *result;
        std::cout << "Player " << clientId << " moved to " << pkt.x << ", " << pkt.y << std::endl;
    }
}
```

### 4. Send Packets

```cpp
// Client sends to server
ChatMessagePacket msg;
strcpy(msg.message, "Hello!");
net.send(msg, true);  // true = reliable

// Server sends to specific client
net.sendToClient(clientId, msg, true);

// Server broadcasts to all
net.broadcastToAll(msg, true);
```

---

## Core Concepts

### Packet Type Identifier

Every packet **must** start with a type identifier. This is how the library routes packets internally.

```cpp
struct MyPacket {
    PacketType type = PacketType::MY_PACKET;  // ✅ REQUIRED - must be first!
    int data;
    float value;
};
```

The type is extracted like this internally:
```cpp
uint8_t type = packet.data[0];  // First byte = packet type
```

### Host vs Client

- **Client Mode**: Connects to a remote server, can only send to server and receive from server
- **Host Mode**: Runs both a server AND a client locally
  - The host connects to `127.0.0.1` (themselves)
  - Other clients connect to the host's IP
  - Host can send/receive as both client and server

### Polling vs Callbacks

**NetLib uses polling** - you explicitly ask for packets when you want them:

```cpp
// ❌ OLD WAY (callbacks)
net.registerHandler(PKT_CHAT, [](const ChatPacket& pkt) {
    // Handle here...
});

// ✅ NEW WAY (polling)
void handleChat() {
    while (auto pkt = net.receiveFromServer<ChatPacket>(PKT_CHAT)) {
        // Handle here - YOU control when and where
    }
}
```

Benefits:
- No scattered callbacks across your codebase
- Call handlers wherever you want
- Easy to organize by system/feature
- Clear control flow

---

## Packet Definitions

### Rules for Packets

1. **First member must be the packet type** (`uint8_t` or enum based on it)
2. **Use POD types only** (Plain Old Data - no pointers, no `std::string`, no complex objects)
3. **Fixed-size arrays for strings**: `char name[32]` not `std::string`
4. **Use fixed-size types**: `uint32_t`, `float`, `int16_t` etc.

### Good Packet Examples

```cpp
// Simple packet
struct PingPacket {
    PacketType type = PacketType::PING;
    uint32_t timestamp;
};

// Packet with multiple fields
struct PlayerStatePacket {
    PacketType type = PacketType::PLAYER_STATE;
    uint8_t uuid[16];  // Fixed-size array
    float x, y, z;
    float velX, velY, velZ;
    bool onGround;
};

// Packet with string
struct LoginPacket {
    PacketType type = PacketType::LOGIN;
    char username[32];  // Fixed-size string
    uint32_t version;
};
```

### Bad Packet Examples

```cpp
// ❌ Type not first
struct BadPacket1 {
    int data;
    PacketType type;  // Must be first!
};

// ❌ Using std::string
struct BadPacket2 {
    PacketType type;
    std::string message;  // Not POD!
};

// ❌ Using pointers
struct BadPacket3 {
    PacketType type;
    int* data;  // Pointers don't serialize!
};
```

---

## Client-Side Usage

### Receiving Packets from Server

Use `receiveFromServer<T>()` to poll for specific packet types:

```cpp
void processServerPackets() {
    // Single packet (if only one is expected)
    if (auto pkt = net.receiveFromServer<LoginSuccessPacket>(PKT_LOGIN_SUCCESS)) {
        onLoginSuccess(pkt->playerId, pkt->spawnX, pkt->spawnY);
    }
    
    // Multiple packets (use while loop)
    while (auto pkt = net.receiveFromServer<PlayerStatePacket>(PKT_PLAYER_STATE)) {
        updatePlayer(pkt->playerId, pkt->x, pkt->y);
    }
}
```

### Sending to Server

Use `send()` to send packets to the server:

```cpp
void sendChatMessage(const std::string& message) {
    ChatMessagePacket pkt;
    pkt.type = PacketType::CHAT_MESSAGE;
    strncpy(pkt.message, message.c_str(), 255);
    pkt.message[255] = '\0';
    
    net.send(pkt, true);  // true = reliable delivery
}

void sendMovement(float x, float y) {
    PlayerMovePacket pkt;
    pkt.type = PacketType::PLAYER_MOVE;
    pkt.x = x;
    pkt.y = y;
    
    net.send(pkt, false);  // false = unreliable (fast, can drop)
}
```

### Client Organization Example

```cpp
class ClientNetworkHandler {
public:
    ClientNetworkHandler(NetLib::GameSession<PacketType>& session) : net(session) {}
    
    void processPackets() {
        handleLoginResponse();
        handlePlayerUpdates();
        handleChatMessages();
    }
    
private:
    NetLib::GameSession<PacketType>& net;
    
    void handleLoginResponse() {
        if (auto pkt = net.receiveFromServer<LoginSuccessPacket>(PKT_LOGIN_SUCCESS)) {
            // Handle successful login
        }
        
        if (auto pkt = net.receiveFromServer<LoginFailedPacket>(PKT_LOGIN_FAILED)) {
            // Handle failed login
        }
    }
    
    void handlePlayerUpdates() {
        while (auto pkt = net.receiveFromServer<PlayerStatePacket>(PKT_PLAYER_STATE)) {
            // Update player positions
        }
    }
    
    void handleChatMessages() {
        while (auto pkt = net.receiveFromServer<ChatMessagePacket>(PKT_CHAT)) {
            // Display chat
        }
    }
};
```

---

## Server-Side Usage

### Receiving Packets from Clients

Use `receiveFromClient<T>()` - it returns **both the packet and sender ID**:

```cpp
void processClientPackets() {
    // Receive with sender ID
    while (auto result = net.receiveFromClient<PlayerMovePacket>(PKT_PLAYER_MOVE)) {
        auto [pkt, clientId] = *result;  // Structured binding
        
        // Process movement from this specific client
        processPlayerMovement(clientId, pkt.x, pkt.y);
    }
}
```

### Handling Connections

Use `pollPlayerConnection()` to detect connects/disconnects:

```cpp
void handleConnections() {
    uint32_t peerId;
    bool isConnect;
    
    while (net.pollPlayerConnection(peerId, isConnect)) {
        if (isConnect) {
            std::cout << "Player " << peerId << " connected" << std::endl;
            // They'll send a login packet next
        } else {
            std::cout << "Player " << peerId << " disconnected" << std::endl;
            removePlayer(peerId);
        }
    }
}
```

### Sending to Clients

Three methods for server sending:

```cpp
// 1. Send to specific client
LoginSuccessPacket loginPkt;
loginPkt.playerId = 5;
net.sendToClient(clientId, loginPkt, true);

// 2. Broadcast to all clients
ChatMessagePacket chatPkt;
strcpy(chatPkt.message, "Server shutting down");
net.broadcastToAll(chatPkt, true);

// 3. Send within a loop
for (uint32_t clientId : connectedClients) {
    PlayerListPacket pkt;
    // ... fill packet ...
    net.sendToClient(clientId, pkt, true);
}
```

### Server Organization Example

```cpp
class ServerNetworkHandler {
public:
    ServerNetworkHandler(NetLib::GameSession<PacketType>& session) : net(session) {}
    
    void processPackets() {
        handleConnections();
        handleLogins();
        handlePlayerInput();
        handleChatMessages();
    }
    
    void broadcastGameState() {
        // Send state to all players
        for (auto& player : players) {
            PlayerStatePacket pkt = getPlayerState(player);
            net.broadcastToAll(pkt, false);  // Unreliable for frequent updates
        }
    }
    
private:
    NetLib::GameSession<PacketType>& net;
    
    void handleConnections() {
        uint32_t peerId;
        bool isConnect;
        
        while (net.pollPlayerConnection(peerId, isConnect)) {
            if (isConnect) {
                onPlayerConnect(peerId);
            } else {
                onPlayerDisconnect(peerId);
            }
        }
    }
    
    void handleLogins() {
        while (auto result = net.receiveFromClient<LoginPacket>(PKT_LOGIN)) {
            auto [pkt, clientId] = *result;
            
            if (isUsernameTaken(pkt.username)) {
                sendLoginFailed(clientId, "Username taken");
            } else {
                acceptLogin(clientId, pkt.username);
            }
        }
    }
    
    void handlePlayerInput() {
        while (auto result = net.receiveFromClient<PlayerInputPacket>(PKT_INPUT)) {
            auto [pkt, clientId] = *result;
            processInput(clientId, pkt);
        }
    }
};
```

---

## The GameSession API

### Creating a Session

```cpp
// Template parameter is your packet header type
NetLib::GameSession<PacketType> session;
```

### Starting as Host

```cpp
bool startHost(uint16_t port = 25565, uint32_t maxClients = 32);

// Example
if (session.startHost(25565, 10)) {
    std::cout << "Server started on port 25565" << std::endl;
    // You are now both server and client
}
```

### Connecting as Client

```cpp
bool connectToServer(const char* host, uint16_t port = 25565);

// Example
if (session.connectToServer("192.168.1.100", 25565)) {
    std::cout << "Connected to server" << std::endl;
}
```

### Core Methods

```cpp
// MUST call every frame to process incoming packets
void update();

// Check if hosting
bool isHosting() const;

// Check if you have a player ID (assigned by server)
bool hasPlayerId() const;

// Get your player ID (if assigned)
uint32_t getMyPlayerId() const;

// Cleanup (disconnect and shutdown)
void cleanup();
```

### Client Methods

```cpp
// Receive from server (returns std::optional)
template<typename T>
std::optional<T> receiveFromServer(uint8_t packetType);

// Send to server
template<typename T>
void send(const T& packet, bool reliable = true);
```

### Server Methods (only work if hosting)

```cpp
// Receive from client (returns std::optional with clientId)
template<typename T>
std::optional<std::pair<T, uint32_t>> receiveFromClient(uint8_t packetType);

// Send to specific client
template<typename T>
void sendToClient(uint32_t clientId, const T& packet, bool reliable = true);

// Broadcast to all clients
template<typename T>
void broadcastToAll(const T& packet, bool reliable = true);

// Poll for connection events
bool pollPlayerConnection(uint32_t& peerId, bool& isConnect);
```

---

## Complete Example

### Packets.h

```cpp
#pragma once
#include <cstdint>

enum class PacketType : uint8_t {
    LOGIN_REQUEST = 1,
    LOGIN_SUCCESS = 2,
    CHAT_MESSAGE = 3,
    PLAYER_MOVE = 4
};

struct LoginRequestPacket {
    PacketType type = PacketType::LOGIN_REQUEST;
    char username[32];
};

struct LoginSuccessPacket {
    PacketType type = PacketType::LOGIN_SUCCESS;
    uint32_t playerId;
};

struct ChatMessagePacket {
    PacketType type = PacketType::CHAT_MESSAGE;
    uint32_t senderId;
    char message[256];
};

struct PlayerMovePacket {
    PacketType type = PacketType::PLAYER_MOVE;
    uint32_t playerId;
    float x, y, z;
};
```

### Client.cpp

```cpp
#include "NetworkWrapper.h"
#include "Packets.h"
#include <iostream>

int main() {
    NetLib::GameSession<PacketType> net;
    
    // Connect to server
    if (!net.connectToServer("127.0.0.1", 25565)) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }
    
    // Send login
    LoginRequestPacket login;
    strcpy(login.username, "Player1");
    net.send(login, true);
    
    bool running = true;
    while (running) {
        net.update();
        
        // Handle login success
        if (auto pkt = net.receiveFromServer<LoginSuccessPacket>(static_cast<uint8_t>(PacketType::LOGIN_SUCCESS))) {
            std::cout << "Logged in! My ID: " << pkt->playerId << std::endl;
        }
        
        // Handle chat messages
        while (auto pkt = net.receiveFromServer<ChatMessagePacket>(static_cast<uint8_t>(PacketType::CHAT_MESSAGE))) {
            std::cout << "Chat: " << pkt->message << std::endl;
        }
        
        // Handle player movement
        while (auto pkt = net.receiveFromServer<PlayerMovePacket>(static_cast<uint8_t>(PacketType::PLAYER_MOVE))) {
            std::cout << "Player " << pkt->playerId << " moved to " 
                      << pkt->x << ", " << pkt->y << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    net.cleanup();
    return 0;
}
```

### Server.cpp

```cpp
#include "NetworkWrapper.h"
#include "Packets.h"
#include <iostream>
#include <unordered_map>

struct Player {
    uint32_t id;
    std::string username;
    float x = 0, y = 0, z = 0;
};

int main() {
    NetLib::GameSession<PacketType> net;
    
    if (!net.startHost(25565, 10)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    
    std::unordered_map<uint32_t, Player> players;
    uint32_t nextPlayerId = 1;
    
    std::cout << "Server running on port 25565" << std::endl;
    
    bool running = true;
    while (running) {
        net.update();
        
        // Handle connections
        uint32_t peerId;
        bool isConnect;
        while (net.pollPlayerConnection(peerId, isConnect)) {
            if (isConnect) {
                std::cout << "Player " << peerId << " connected" << std::endl;
            } else {
                std::cout << "Player " << peerId << " disconnected" << std::endl;
                players.erase(peerId);
            }
        }
        
        // Handle login requests
        while (auto result = net.receiveFromClient<LoginRequestPacket>(static_cast<uint8_t>(PacketType::LOGIN_REQUEST))) {
            auto [pkt, clientId] = *result;
            
            Player player;
            player.id = nextPlayerId++;
            player.username = pkt.username;
            players[clientId] = player;
            
            // Send success
            LoginSuccessPacket success;
            success.playerId = player.id;
            net.sendToClient(clientId, success, true);
            
            // Announce to all
            ChatMessagePacket announce;
            announce.senderId = 0;
            snprintf(announce.message, 256, "%s joined the game", player.username.c_str());
            net.broadcastToAll(announce, true);
            
            std::cout << player.username << " logged in (ID: " << player.id << ")" << std::endl;
        }
        
        // Handle movement
        while (auto result = net.receiveFromClient<PlayerMovePacket>(static_cast<uint8_t>(PacketType::PLAYER_MOVE))) {
            auto [pkt, clientId] = *result;
            
            auto it = players.find(clientId);
            if (it != players.end()) {
                it->second.x = pkt.x;
                it->second.y = pkt.y;
                it->second.z = pkt.z;
                
                // Broadcast to all other players
                net.broadcastToAll(pkt, false);  // Unreliable for movement
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    net.cleanup();
    return 0;
}
```

---

## Best Practices

### Packet Design

1. **Keep packets small** - Under 1KB is ideal
2. **Separate frequent from infrequent data** - Don't send inventory data with position updates
3. **Use bit fields for booleans**:
   ```cpp
   struct Flags {
       bool jumping : 1;
       bool crouching : 1;
       bool sprinting : 1;
   };
   ```
4. **Version your packets** - Include a version field for future compatibility

### Reliable vs Unreliable

**Reliable (`true`):**
- Login/logout
- Chat messages
- Important game events (player died, item picked up)
- Anything that must arrive

**Unreliable (`false`):**
- Position updates (send frequently, okay if some drop)
- Rotation/look direction
- Sound effects
- Non-critical visual effects

### Game Loop Integration

```cpp
void gameLoop() {
    const float TICK_RATE = 1.0f / 20.0f;  // 20 Hz
    float accumulator = 0.0f;
    
    while (running) {
        float deltaTime = getFrameDeltaTime();
        accumulator += deltaTime;
        
        // Process network FIRST
        net.update();
        clientNetHandler.processPackets();
        
        if (isServer) {
            serverNetHandler.processPackets();
        }
        
        // Fixed tick for server simulation
        while (accumulator >= TICK_RATE) {
            if (isServer) {
                serverGame.tick(TICK_RATE);
                serverNetHandler.broadcastGameState();
            }
            accumulator -= TICK_RATE;
        }
        
        // Render
        clientGame.render();
    }
}
```

### Error Handling

```cpp
// Always check connection status
if (!net.connectToServer(host, port)) {
    showError("Failed to connect");
    return;
}

// Check if logged in before sending gameplay packets
if (!isLoggedIn) return;

// Validate packet data
if (pkt->username[0] == '\0') {
    // Invalid username
}
```

### Organizing Network Code

**Separate your concerns:**

```
Client/
  - ClientGame.cpp          // Game logic, rendering
  - ClientNetHandler.cpp    // Polls packets, calls ClientGame methods

Server/
  - ServerGame.cpp          // Authority, physics, validation
  - ServerNetHandler.cpp    // Polls packets, calls ServerGame methods

Game.cpp                    // Coordinator, creates client/server
```

**Don't mix:**
- ❌ Client rendering code in server files
- ❌ Server authority logic in client files
- ❌ Network polling mixed with game logic

---

## API Reference

### GameSession<PacketHeaderType>

**Constructor:**
```cpp
GameSession()
```

**Connection:**
```cpp
bool startHost(uint16_t port = 25565, uint32_t maxClients = 32)
bool connectToServer(const char* host, uint16_t port = 25565)
void cleanup()
```

**Update:**
```cpp
void update()  // Call every frame!
```

**Client Methods:**
```cpp
template<typename T>
std::optional<T> receiveFromServer(uint8_t packetType)

template<typename T>
void send(const T& packet, bool reliable = true)
```

**Server Methods:**
```cpp
template<typename T>
std::optional<std::pair<T, uint32_t>> receiveFromClient(uint8_t packetType)

template<typename T>
void sendToClient(uint32_t clientId, const T& packet, bool reliable = true)

template<typename T>
void broadcastToAll(const T& packet, bool reliable = true)

bool pollPlayerConnection(uint32_t& peerId, bool& isConnect)
```

**State Queries:**
```cpp
bool isHosting() const
bool hasPlayerId() const
uint32_t getMyPlayerId() const
```

---

## Troubleshooting

### "No packets received"
- Did you call `net.update()` every frame?
- Is the packet type correct? (Check first byte matches enum)
- Using `while` instead of `if` for multiple packets?

### "Connection failed"
- Is the server running?
- Firewall blocking the port?
- Correct IP address?

### "Packets not arriving"
- If unreliable, some packets can drop (expected)
- If reliable, check network stability
- Are you sending too much data? (fragmentation issues)

### "Host can't see themselves"
- Host needs to handle packets from their own client
- Don't skip packets where `clientId == 0` on host

---

## License

MIT License - See LICENSE file for details

## Credits

Built on top of [ENet](http://enet.bespin.org/) by Lee Salzman