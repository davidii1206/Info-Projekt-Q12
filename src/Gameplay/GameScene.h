#pragma once
#include "Scene.h"
#include <unordered_map>
#include <cstdint>

class GameScene : public IScene {
public:
    const char* Name() const override { return "GameScene"; }
    void OnEnter(SceneContext& ctx) override;
    void OnExit(SceneContext& ctx)  override;
    void FrameUpdate(SceneContext& ctx, float dt) override;
    void FixedUpdate(SceneContext& ctx, float dt) override;

private:
    // --- server-side helpers (only called when hosting) ---
    void PollConnectionEvents(SceneContext& ctx);
    void PollClientPackets(SceneContext& ctx);
    void SendSnapshots(SceneContext& ctx);

    // --- client-side helpers (always called when connected) ---
    void PollServerPackets(SceneContext& ctx);
    void SendLocalInput(SceneContext& ctx);

    // Server state
    uint32_t m_NextNetId    = 1;
    uint32_t m_NextPlayerId = 0;
    std::unordered_map<uint32_t, entt::entity> m_ServerNetMap; // netId → server entity
    std::unordered_map<uint32_t, uint32_t>     m_PeerToNetId;  // peerId → netId

    // Client state
    uint32_t m_MyPlayerId = 0;
    uint32_t m_MyNetId    = 0;
    bool     m_IdAssigned = false;
    std::unordered_map<uint32_t, entt::entity> m_ClientNetMap; // netId → client entity

    float m_SnapAccum = 0.f;
    static constexpr float SNAPSHOT_RATE = 1.f / 20.f; // send snapshots at 20 Hz
};
