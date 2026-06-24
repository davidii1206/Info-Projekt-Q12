#include "LobbyScene.h"
#include "GameScene.h"
#include "MainMenuScene.h"
#include "../Networking/NetworkManager.h"
#include "../Core/Input.h"
#include "../Graphics/Renderer.h"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <cstring>
#include <unordered_set>

LobbyScene::LobbyScene() {}
LobbyScene::~LobbyScene() {}

void LobbyScene::OnEnter(SceneContext& ctx) {
    spdlog::info("LobbyScene: entered");
    m_SelectedClass = BugClass::Ants;
    m_Ready = false;
    m_GameStarting = false;
    m_LobbyPlayers.clear();

    // Check if we already have player entities (returning from GameScene).
    // Rebuild m_ServerNetMap, m_PeerToNetId and the netId/playerId counters
    // so the next CONNECT event doesn't reuse already-assigned IDs and
    // existing peers still match incoming LOBBY_UPDATE packets.
    bool hasExistingPlayers = false;
    uint32_t maxNetId    = 0;
    uint32_t maxPlayerId = 0;
    bool     anyPlayer   = false;
    {
        auto pView = ctx.serverRegistry.view<PlayerComponent>();
        for (auto e : pView) {
            hasExistingPlayers = true;
            auto& pc = pView.get<PlayerComponent>(e);
            auto* nc = ctx.serverRegistry.try_get<NetworkedComponent>(e);
            LobbyPlayerInfo info;
            info.playerId = pc.playerId;
            info.netId    = nc ? nc->netId : 0;
            info.bugClass = BugClass::None; // Reset class for new round
            info.ready    = false;            // Reset ready
            m_LobbyPlayers[info.playerId] = info;

            // Rebuild net maps if needed
            if (nc) m_ServerNetMap[nc->netId] = e;
            // Rebuild peer<->net map from persisted peerId (only host stamps it)
            if (pc.peerId != 0xFFFFFFFFu && nc)
                m_PeerToNetId[pc.peerId] = nc->netId;

            anyPlayer = true;
            if (nc && nc->netId > maxNetId)       maxNetId    = nc->netId;
            if (pc.playerId > maxPlayerId)        maxPlayerId = pc.playerId;

            if (pc.isLocal) {
                m_MyPlayerId = pc.playerId;
                m_MyNetId    = nc ? nc->netId : 0;
                m_IdAssigned = true;
            }
        }
    }
    // Continue assigning fresh IDs above the highest one already in use.
    if (anyPlayer) {
        m_NextNetId    = maxNetId + 1;
        m_NextPlayerId = maxPlayerId + 1;
    }

    // If not hosting, try to recover identity from client registry (returning from game)
    if (!hasExistingPlayers && !ctx.network.IsHosting()) {
        auto cView = ctx.clientRegistry.view<PlayerComponent>();
        for (auto e : cView) {
            auto& pc = cView.get<PlayerComponent>(e);
            if (pc.isLocal) {
                m_MyPlayerId = pc.playerId;
                m_IdAssigned = true;
                for (auto& [netId, entity] : m_ClientNetMap) {
                    if (entity == e) {
                        m_MyNetId = netId;
                        break;
                    }
                }
                spdlog::info("LobbyScene: recovered client identity playerId={} netId={}",
                             m_MyPlayerId, m_MyNetId);
                break;
            }
        }
    }

    // Broadcast lobby state to sync all clients
    if (ctx.network.IsHosting()) {
        // Don't create a host player here — the host's internal loopback
        // connection (GameSession::startHost creates a Client connected to
        // 127.0.0.1) will fire a CONNECT event that PollConnectionEvents
        // picks up naturally. That flow also sends a PlayerIdAssignPacket
        // back so the host learns their own playerId.
        BroadcastLobbyState(ctx);
    }
}

void LobbyScene::OnExit(SceneContext& ctx) {
    spdlog::info("LobbyScene: exited");
}

void LobbyScene::LogicUpdate(SceneContext& ctx, float dt) {
    if (!ctx.network.IsConnected()) {
        ctx.scenes.RequestTransition(new MainMenuScene());
        return;
    }
}

void LobbyScene::UIUpdate(SceneContext& ctx, float /*dt*/) {
    if (m_GameStarting) {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("Game Starting", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::Text("Game is starting...");
        ImGui::End();
        return;
    }

    ImGui::Begin("Lobby");
    ImGui::Text("Bugmin - Lobby");
    ImGui::Separator();

    // Player list
    if (ImGui::BeginTable("PlayerList", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Player");
        ImGui::TableSetupColumn("Bug Class");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Host");
        ImGui::TableHeadersRow();

        for (auto& [pid, info] : m_LobbyPlayers) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            bool isMe = (pid == m_MyPlayerId);
            ImGui::Text("%s%s", isMe ? "You" : "Player", "");
            if (isMe) ImGui::SameLine();
            ImGui::TextDisabled("(%u)", pid);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", BugClassName(info.bugClass));

            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(info.ready ? ImVec4(0,1,0,1) : ImVec4(1,1,0,1),
                               info.ready ? "Ready" : "Waiting");

            ImGui::TableSetColumnIndex(3);
            if (pid == 0 && ctx.network.IsHosting())
                ImGui::Text("Host");
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::Text("Your Class:");
    const BugClass availableClasses[] = {
        BugClass::Ants, BugClass::BeesWasps, BugClass::Beetles,
        BugClass::Bugs, BugClass::ButterfliesMoths, BugClass::CentipedesWorms,
        BugClass::Dragonflies, BugClass::Fireflies, BugClass::Mantis,
        BugClass::MosquitosTicks, BugClass::Roaches, BugClass::Scorpions,
        BugClass::Snails, BugClass::Spiders, BugClass::Termites,
        BugClass::Woodlice
    };
    // Build set of classes already taken by other players
    std::unordered_set<BugClass> takenClasses;
    for (auto& [pid, info] : m_LobbyPlayers) {
        if (pid != m_MyPlayerId && info.bugClass != BugClass::None)
            takenClasses.insert(info.bugClass);
    }
    const char* preview = BugClassName(m_SelectedClass);
    if (ImGui::BeginCombo("##bugclass", preview)) {
        for (auto bc : availableClasses) {
            bool isSelected   = (bc == m_SelectedClass);
            bool isTaken      = takenClasses.count(bc) > 0;
            if (isTaken && !isSelected) continue; // hide already-taken classes
            if (!ImGui::Selectable(BugClassName(bc), isSelected, isTaken ? ImGuiSelectableFlags_Disabled : 0))
                continue;
            m_SelectedClass = bc;
            if (m_IdAssigned) {
                LobbyUpdatePacket pkt;
                pkt.playerId = m_MyPlayerId;
                pkt.bugClass = static_cast<uint8_t>(bc);
                pkt.ready    = m_Ready ? 1 : 0;
                ctx.network.Send(pkt);
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Ready toggle
    if (ImGui::Button(m_Ready ? "Not Ready" : "Ready")) {
        m_Ready = !m_Ready;
        if (m_IdAssigned) {
            LobbyUpdatePacket pkt;
            pkt.playerId = m_MyPlayerId;
            pkt.bugClass = static_cast<uint8_t>(m_SelectedClass);
            pkt.ready    = m_Ready ? 1 : 0;
            ctx.network.Send(pkt);
        }
    }

    // Host-only controls
    if (ctx.network.IsHosting()) {
        ImGui::Separator();
        bool allReady = true;
        for (auto& [pid, info] : m_LobbyPlayers)
            if (!info.ready) { allReady = false; break; }
        // Solo playtest is allowed; the lobby just needs at least the host.
        if (m_LobbyPlayers.empty())
            allReady = false;

        if (ImGui::Button("Start Game") && allReady && !m_GameStarting) {
            m_GameStarting = true;
            spdlog::info("LobbyScene: host starting game...");

            // Assign bug classes to player entities
            for (auto& [pid, info] : m_LobbyPlayers) {
                auto it = m_ServerNetMap.find(info.netId);
                if (it == m_ServerNetMap.end()) continue;
                auto* pc = ctx.serverRegistry.try_get<PlayerComponent>(it->second);
                if (pc) {
                    pc->bugClass = info.bugClass;
                    pc->cameraMode = CameraMode::Building;
                }
            }

            // Broadcast game start
            ctx.network.BroadcastToAll(GameStartPacket{});

            // Host transitions immediately
            ctx.scenes.RequestTransition(new GameScene());
        }
        if (!allReady && !m_LobbyPlayers.empty())
            ImGui::TextDisabled("Waiting for all players to ready up...");
        else if (m_LobbyPlayers.empty())
            ImGui::TextDisabled("Waiting for the host to join the lobby...");
    }

    ImGui::Separator();
    if (ImGui::Button("Disconnect")) {
        ctx.network.Disconnect();
        ctx.scenes.RequestTransition(new MainMenuScene());
    }
    ImGui::End();
}

void LobbyScene::FixedUpdate(SceneContext& ctx, float dt) {
    if (ctx.network.IsHosting()) {
        PollConnectionEvents(ctx);
        PollClientPackets(ctx);
    }

    if (ctx.network.IsConnected()) {
        PollServerPackets(ctx);
    }
}

void LobbyScene::Render(SceneContext& /*ctx*/, Renderer* renderer) {
    int w, h;
    SDL_GetWindowSizeInPixels(renderer->GetWindow()->handle, &w, &h);
    float aspect = (float)w / (float)h;

    GlobalUniforms& globals = renderer->GetGlobalUniforms();
    globals.view = glm::mat4(1.f);
    globals.proj = glm::mat4(1.f);
    globals.viewProj = glm::mat4(1.f);
    globals.cameraPos = glm::vec4(0.f, 0.f, 0.f, 1.f);

    renderer->UpdateGlobalUniforms(globals);
}

void LobbyScene::PollConnectionEvents(SceneContext& ctx) {
    uint32_t peerId;
    bool     isConnect;

    while (ctx.network.PollPlayerConnection(peerId, isConnect)) {
        if (isConnect) {
            const uint32_t newNetId    = m_NextNetId++;
            const uint32_t newPlayerId = m_NextPlayerId++;

            auto entity = ctx.serverRegistry.create();
            ctx.serverRegistry.emplace<TransformComponent>(entity);
            ctx.serverRegistry.emplace<MovementComponent>(entity);
            // Stamp peerId so the mapping survives scene transitions.
            PlayerComponent pc{};
            pc.playerId = newPlayerId;
            pc.isLocal  = false;
            pc.peerId   = peerId;
            ctx.serverRegistry.emplace<PlayerComponent>(entity, pc);
            ctx.serverRegistry.emplace<NetworkedComponent>(entity, newNetId);
            m_ServerNetMap[newNetId] = entity;
            m_PeerToNetId[peerId]    = newNetId;

            // Tell the new client their identity
            PlayerIdAssignPacket idPkt;
            idPkt.playerId = newPlayerId;
            idPkt.netId    = newNetId;
            ctx.network.SendToClient(peerId, idPkt);

            // Broadcast the new player to all clients
            PlayerJoinedPacket broadcastPkt;
            broadcastPkt.netId    = newNetId;
            broadcastPkt.playerId = newPlayerId;
            ctx.network.BroadcastToAll(broadcastPkt);

            // Add to lobby state
            LobbyPlayerInfo info;
            info.playerId = newPlayerId;
            info.netId    = newNetId;
            info.bugClass = BugClass::None;
            info.ready    = false;
            m_LobbyPlayers[newPlayerId] = info;

            spdlog::info("LobbyScene: player joined  peerId={} playerId={} netId={}",
                         peerId, newPlayerId, newNetId);
        } else {
            auto it = m_PeerToNetId.find(peerId);
            if (it != m_PeerToNetId.end()) {
                const uint32_t netId = it->second;
                // Find playerId for this netId
                uint32_t leftPlayerId = 0;
                for (auto& [pid, info] : m_LobbyPlayers) {
                    if (info.netId == netId) {
                        leftPlayerId = pid;
                        break;
                    }
                }
                ctx.serverRegistry.destroy(m_ServerNetMap[netId]);
                m_ServerNetMap.erase(netId);
                m_PeerToNetId.erase(peerId);
                m_LobbyPlayers.erase(leftPlayerId);

                PlayerLeftPacket pkt;
                pkt.netId = netId;
                ctx.network.BroadcastToAll(pkt);

                spdlog::info("LobbyScene: player left  peerId={} netId={}", peerId, netId);
            }
        }

        // Broadcast updated lobby state to everyone after any change
        BroadcastLobbyState(ctx);
    }
}

void LobbyScene::PollClientPackets(SceneContext& ctx) {
    while (true) {
        auto result = ctx.network.ReceiveFromClient<LobbyUpdatePacket>(PacketType::LOBBY_UPDATE);
        if (!result) break;
        auto [pkt, senderPeer] = *result;

        auto it = m_PeerToNetId.find(senderPeer);
        if (it == m_PeerToNetId.end()) continue;

        uint32_t senderNetId = it->second;
        uint32_t senderPlayerId = 0;
        for (auto& [pid, info] : m_LobbyPlayers) {
            if (info.netId == senderNetId) {
                senderPlayerId = pid;
                break;
            }
        }

        if (m_LobbyPlayers.count(senderPlayerId)) {
            auto& info = m_LobbyPlayers[senderPlayerId];

            // Check bug class uniqueness
            BugClass requestedClass = static_cast<BugClass>(pkt.bugClass);
            bool classTaken = false;
            if (requestedClass != BugClass::None && requestedClass != info.bugClass) {
                for (auto& [pid, other] : m_LobbyPlayers) {
                    if (pid != senderPlayerId && other.bugClass == requestedClass) {
                        classTaken = true;
                        break;
                    }
                }
            }
            if (!classTaken) {
                info.bugClass = requestedClass;
            }
            info.ready = pkt.ready != 0;

            spdlog::debug("LobbyScene: player {} class={} ready={}",
                          senderPlayerId, BugClassName(info.bugClass), info.ready);
        }

        BroadcastLobbyState(ctx);
    }
}

void LobbyScene::PollServerPackets(SceneContext& ctx) {
    // Player ID assignment
    if (!m_IdAssigned) {
        auto idPkt = ctx.network.ReceiveFromServer<PlayerIdAssignPacket>(PacketType::PLAYER_ID_ASSIGN);
        if (idPkt) {
            m_MyPlayerId = idPkt->playerId;
            m_MyNetId    = idPkt->netId;
            m_IdAssigned = true;
            spdlog::info("LobbyScene: assigned playerId={} netId={}", m_MyPlayerId, m_MyNetId);
        }
    }

    // Player joined
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<PlayerJoinedPacket>(PacketType::PLAYER_JOINED);
        if (!pkt) break;
        if (m_ClientNetMap.count(pkt->netId)) continue;
        auto entity = ctx.clientRegistry.create();
        ctx.clientRegistry.emplace<TransformComponent>(entity, glm::vec3{pkt->x, pkt->y, pkt->z});
        ctx.clientRegistry.emplace<MovementComponent>(entity);
        ctx.clientRegistry.emplace<PlayerComponent>(entity, pkt->playerId, pkt->playerId == m_MyPlayerId);
        ctx.clientRegistry.emplace<NetworkedComponent>(entity, pkt->netId);
        m_ClientNetMap[pkt->netId] = entity;
    }

    // Player left
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<PlayerLeftPacket>(PacketType::PLAYER_LEFT);
        if (!pkt) break;
        auto it = m_ClientNetMap.find(pkt->netId);
        if (it != m_ClientNetMap.end()) {
            ctx.clientRegistry.destroy(it->second);
            m_ClientNetMap.erase(it);
        }
    }

    // Lobby state update from server
    while (true) {
        auto pkt = ctx.network.ReceiveFromServer<LobbyStatePacket>(PacketType::LOBBY_STATE);
        if (!pkt) break;
        m_LobbyPlayers.clear();
        for (uint32_t i = 0; i < pkt->playerCount; ++i) {
            const auto& src = pkt->players[i];
            LobbyPlayerInfo info;
            info.playerId = src.playerId;
            info.netId    = src.netId;
            info.bugClass = static_cast<BugClass>(src.bugClass);
            info.ready    = src.ready != 0;
            m_LobbyPlayers[info.playerId] = info;

            // If this is us, sync our local selected class
            if (info.playerId == m_MyPlayerId) {
                m_SelectedClass = info.bugClass != BugClass::None
                    ? info.bugClass : m_SelectedClass;
                m_Ready = info.ready;
            }
        }
    }

    // Game start signal
    {
        auto pkt = ctx.network.ReceiveFromServer<GameStartPacket>(PacketType::GAME_START);
        if (pkt && !m_GameStarting) {
            m_GameStarting = true;
            spdlog::info("LobbyScene: game start received, transitioning to GameScene");
            ctx.scenes.RequestTransition(new GameScene());
        }
    }
}

void LobbyScene::BroadcastLobbyState(SceneContext& ctx) {
    LobbyStatePacket pkt;
    pkt.playerCount = static_cast<uint32_t>(m_LobbyPlayers.size());

    uint32_t idx = 0;
    for (auto& [pid, info] : m_LobbyPlayers) {
        if (idx >= LobbyStatePacket::MAX_PLAYERS) break;
        pkt.players[idx].playerId = info.playerId;
        pkt.players[idx].netId    = info.netId;
        pkt.players[idx].bugClass = static_cast<uint8_t>(info.bugClass);
        pkt.players[idx].ready    = info.ready ? 1 : 0;
        ++idx;
    }

    ctx.network.BroadcastToAll(pkt);
}
