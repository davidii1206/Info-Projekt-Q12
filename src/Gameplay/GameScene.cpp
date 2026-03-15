#include "GameScene.h"
#include "MainMenuScene.h"
#include "Components.h"
#include "Systems.h"
#include "../Networking/NetworkManager.h"
#include "../Networking/Packets.h"
#include "../Core/Input.h"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void GameScene::OnEnter(SceneContext& ctx) {
    spdlog::info("GameScene: entered");
    // Player entities are created reactively via connection events and packets.
}

void GameScene::OnExit(SceneContext& ctx) {
    ctx.serverRegistry.clear();
    ctx.clientRegistry.clear();
    m_ServerNetMap.clear();
    m_PeerToNetId.clear();
    m_ClientNetMap.clear();
    m_NextNetId     = 1;
    m_NextPlayerId  = 0;
    m_MyPlayerId    = 0;
    m_MyNetId       = 0;
    m_IdAssigned    = false;
    m_SnapAccum     = 0.f;
    spdlog::info("GameScene: exited, registries cleared");
}

// ---------------------------------------------------------------------------
// Per-frame update (ImGui only — no game logic here)
// ---------------------------------------------------------------------------

void GameScene::FrameUpdate(SceneContext& ctx, float dt) {
    // If connection was lost externally (e.g. NetworkDebugUI disconnect button),
    // return to main menu automatically.
    if (!ctx.network.IsConnected()) {
        ctx.scenes.RequestTransition(new MainMenuScene());
        return;
    }

    ImGui::Begin("Game");
    ImGui::Text("Mode: %s", ctx.network.IsHosting() ? "Host" : "Client");
    if (m_IdAssigned)
        ImGui::Text("playerId=%u  netId=%u", m_MyPlayerId, m_MyNetId);
    else
        ImGui::Text("Waiting for server assignment...");
    if (ImGui::Button("Disconnect")) {
        ctx.network.Disconnect();
        ctx.scenes.RequestTransition(new MainMenuScene());
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Fixed-rate update (logic + networking)
// ---------------------------------------------------------------------------

void GameScene::FixedUpdate(SceneContext& ctx, float dt) {
    if (ctx.network.IsHosting()) {
        PollConnectionEvents(ctx);
        PollClientPackets(ctx);
    }

    if (ctx.network.IsConnected()) {
        PollServerPackets(ctx);
    }

    if (ctx.network.IsHosting()) {
        Systems::MovementSystem(ctx.serverRegistry, dt);
    }
    Systems::MovementSystem(ctx.clientRegistry, dt);

    SendLocalInput(ctx);

    if (ctx.network.IsHosting()) {
        m_SnapAccum += dt;
        if (m_SnapAccum >= SNAPSHOT_RATE) {
            SendSnapshots(ctx);
            m_SnapAccum -= SNAPSHOT_RATE;
        }
    }
}

// ---------------------------------------------------------------------------
// Server-side: connection / disconnection
// ---------------------------------------------------------------------------

void GameScene::PollConnectionEvents(SceneContext& ctx) {
    uint32_t peerId;
    bool     isConnect;

    while (ctx.network.PollPlayerConnection(peerId, isConnect)) {
        if (isConnect) {
            const uint32_t newNetId    = m_NextNetId++;
            const uint32_t newPlayerId = m_NextPlayerId++;

            // 1. Send all currently existing entities to the new client (late-join sync).
            //    Done BEFORE adding the new entity so there are no duplicates.
            for (auto& [netId, ent] : m_ServerNetMap) {
                auto& t = ctx.serverRegistry.get<TransformComponent>(ent);
                auto& p = ctx.serverRegistry.get<PlayerComponent>(ent);
                PlayerJoinedPacket pkt;
                pkt.netId    = netId;
                pkt.playerId = p.playerId;
                pkt.x = t.position.x; pkt.y = t.position.y; pkt.z = t.position.z;
                ctx.network.SendToClient(peerId, pkt);
            }

            // 2. Tell the new client their own identity.
            PlayerIdAssignPacket idPkt;
            idPkt.playerId = newPlayerId;
            idPkt.netId    = newNetId;
            ctx.network.SendToClient(peerId, idPkt);

            // 3. Create the server entity.
            auto entity = ctx.serverRegistry.create();
            ctx.serverRegistry.emplace<TransformComponent>(entity);
            ctx.serverRegistry.emplace<MovementComponent>(entity);
            ctx.serverRegistry.emplace<PlayerComponent>(entity, newPlayerId, false);
            ctx.serverRegistry.emplace<NetworkedComponent>(entity, newNetId);
            m_ServerNetMap[newNetId] = entity;
            m_PeerToNetId[peerId]    = newNetId;

            // 4. Broadcast the new entity to ALL clients (including the new one).
            PlayerJoinedPacket broadcastPkt;
            broadcastPkt.netId    = newNetId;
            broadcastPkt.playerId = newPlayerId;
            ctx.network.BroadcastToAll(broadcastPkt);

            spdlog::info("GameScene: player joined  peerId={} playerId={} netId={}",
                         peerId, newPlayerId, newNetId);
        } else {
            auto it = m_PeerToNetId.find(peerId);
            if (it != m_PeerToNetId.end()) {
                const uint32_t netId = it->second;

                ctx.serverRegistry.destroy(m_ServerNetMap[netId]);
                m_ServerNetMap.erase(netId);
                m_PeerToNetId.erase(peerId);

                PlayerLeftPacket pkt;
                pkt.netId = netId;
                ctx.network.BroadcastToAll(pkt);

                spdlog::info("GameScene: player left  peerId={} netId={}", peerId, netId);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Server-side: process client input
// ---------------------------------------------------------------------------

void GameScene::PollClientPackets(SceneContext& ctx) {
    while (true) {
        auto result = ctx.network.ReceiveFromClient<PlayerInputPacket>(PacketType::PLAYER_INPUT);
        if (!result) break;
        auto [packet, senderPeerId] = *result;

        auto peerIt = m_PeerToNetId.find(senderPeerId);
        if (peerIt == m_PeerToNetId.end()) continue;

        auto entIt = m_ServerNetMap.find(peerIt->second);
        if (entIt == m_ServerNetMap.end()) continue;

        auto* movement = ctx.serverRegistry.try_get<MovementComponent>(entIt->second);
        if (movement) movement->inputDir = {packet.dx, packet.dz};
    }
}

// ---------------------------------------------------------------------------
// Server-side: broadcast snapshots to all clients
// ---------------------------------------------------------------------------

void GameScene::SendSnapshots(SceneContext& ctx) {
    auto view = ctx.serverRegistry.view<NetworkedComponent, TransformComponent, MovementComponent>();
    for (auto entity : view) {
        auto& net = view.get<NetworkedComponent>(entity);
        auto& t   = view.get<TransformComponent>(entity);
        auto& m   = view.get<MovementComponent>(entity);

        EntitySnapshotPacket pkt;
        pkt.netId = net.netId;
        pkt.x = t.position.x; pkt.y = t.position.y; pkt.z = t.position.z;
        pkt.vx = m.velocity.x; pkt.vy = m.velocity.y; pkt.vz = m.velocity.z;
        ctx.network.BroadcastToAll(pkt);
    }
}

// ---------------------------------------------------------------------------
// Client-side: process all incoming server packets
// ---------------------------------------------------------------------------

void GameScene::PollServerPackets(SceneContext& ctx) {
    // Identity assignment — only arrives once after connecting.
    if (!m_IdAssigned) {
        auto idPkt = ctx.network.ReceiveFromServer<PlayerIdAssignPacket>(PacketType::PLAYER_ID_ASSIGN);
        if (idPkt) {
            m_MyPlayerId = idPkt->playerId;
            m_MyNetId    = idPkt->netId;
            m_IdAssigned = true;
            spdlog::info("GameScene: assigned playerId={} netId={}", m_MyPlayerId, m_MyNetId);
        }
    }

    // New entities (world state sync + new joins during session).
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<PlayerJoinedPacket>(PacketType::PLAYER_JOINED);
        if (!pkt) break;
        if (m_ClientNetMap.count(pkt->netId)) continue; // duplicate guard

        auto entity = ctx.clientRegistry.create();
        ctx.clientRegistry.emplace<TransformComponent>(entity, glm::vec3{pkt->x, pkt->y, pkt->z});
        ctx.clientRegistry.emplace<MovementComponent>(entity);
        ctx.clientRegistry.emplace<PlayerComponent>(entity, pkt->playerId,
                                                     pkt->playerId == m_MyPlayerId);
        ctx.clientRegistry.emplace<NetworkedComponent>(entity, pkt->netId);
        m_ClientNetMap[pkt->netId] = entity;

        spdlog::info("GameScene: client entity created  netId={} playerId={}",
                     pkt->netId, pkt->playerId);
    }

    // Removed entities.
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<PlayerLeftPacket>(PacketType::PLAYER_LEFT);
        if (!pkt) break;
        auto it = m_ClientNetMap.find(pkt->netId);
        if (it != m_ClientNetMap.end()) {
            ctx.clientRegistry.destroy(it->second);
            m_ClientNetMap.erase(it);
        }
    }

    // Position/velocity snapshots — teammate uses these for interpolation.
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<EntitySnapshotPacket>(PacketType::ENTITY_SNAPSHOT);
        if (!pkt) break;
        auto it = m_ClientNetMap.find(pkt->netId);
        if (it == m_ClientNetMap.end()) continue;

        auto* t = ctx.clientRegistry.try_get<TransformComponent>(it->second);
        auto* m = ctx.clientRegistry.try_get<MovementComponent>(it->second);
        if (t) t->position = {pkt->x,  pkt->y,  pkt->z};
        if (m) m->velocity  = {pkt->vx, pkt->vy, pkt->vz};
    }
}

// ---------------------------------------------------------------------------
// Client-side: send this client's movement input to the server each tick
// ---------------------------------------------------------------------------

void GameScene::SendLocalInput(SceneContext& ctx) {
    if (!ctx.network.IsConnected()) return;

    glm::vec2 dir{0.f};
    if (Input::IsKeyDown(SDLK_W)) dir.y -= 1.f;
    if (Input::IsKeyDown(SDLK_S)) dir.y += 1.f;
    if (Input::IsKeyDown(SDLK_A)) dir.x -= 1.f;
    if (Input::IsKeyDown(SDLK_D)) dir.x += 1.f;
    if (glm::length(dir) > 0.f) dir = glm::normalize(dir);

    PlayerInputPacket pkt;
    pkt.dx = dir.x;
    pkt.dz = dir.y;
    ctx.network.Send(pkt);
}
