#pragma once
#include "Scene.h"
#include "../Networking/Packets.h"
#include "Bug_classes.h"
#include <unordered_map>
#include <cstdint>

class LobbyScene : public IScene {
public:
    LobbyScene();
    ~LobbyScene() override;

    const char* Name() const override { return "LobbyScene"; }

    void OnEnter(SceneContext& ctx) override;
    void OnExit(SceneContext& ctx)  override;
    void LogicUpdate(SceneContext& ctx, float dt) override;
    void UIUpdate(SceneContext& ctx, float dt) override;
    void FixedUpdate(SceneContext& ctx, float dt) override;
    void Render(SceneContext& ctx, Renderer* renderer) override;

private:
    void PollConnectionEvents(SceneContext& ctx);
    void PollClientPackets(SceneContext& ctx);
    void PollServerPackets(SceneContext& ctx);
    void BroadcastLobbyState(SceneContext& ctx);

    struct LobbyPlayerInfo {
        uint32_t playerId  = 0;
        uint32_t netId     = 0;
        BugClass bugClass  = BugClass::None;
        bool     ready     = false;
    };

    std::unordered_map<uint32_t, LobbyPlayerInfo> m_LobbyPlayers;
    BugClass m_SelectedClass  = BugClass::Ants;
    bool     m_Ready          = false;
    bool     m_GameStarting   = false;

    uint32_t m_NextNetId    = 1;
    uint32_t m_NextPlayerId = 0;
    std::unordered_map<uint32_t, uint32_t> m_PeerToNetId;
    std::unordered_map<uint32_t, entt::entity> m_ServerNetMap;

    uint32_t m_MyPlayerId = 0;
    uint32_t m_MyNetId    = 0;
    bool     m_IdAssigned = false;
    std::unordered_map<uint32_t, entt::entity> m_ClientNetMap;
};
