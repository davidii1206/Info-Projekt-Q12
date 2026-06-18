#include "GameLayer.h"
#include "../Graphics/Renderer.h"
#include "../Gameplay/WorldDebugUI.h"
#include "../Core/Timer.h"
#include <spdlog/spdlog.h>

GameLayer::GameLayer(PhysicsServer* physics, NetworkManager* network, Renderer* renderer, Timer* timer)
    : Layer("GameLayer"), m_Physics(physics), m_Network(network), m_Renderer(renderer), m_Timer(timer)
{
    m_World = std::make_unique<World>(m_Physics);
    m_PostProcessor = std::make_unique<PostProcessor>(renderer);
    m_World->SetPostProcessor(m_PostProcessor.get());
}

void GameLayer::OnAttach() {
    // Biome-Sounds registration
    auto& sound = SoundSystem::Get();
    sound.RegisterBiomeSound(BiomeType::MushroomForest, "src/Assets/audio/ambient_forest.wav");
    sound.RegisterBiomeSound(BiomeType::Desert,         "src/Assets/audio/ambient_desert.wav");
    sound.RegisterBiomeSound(BiomeType::Wetland,        "src/Assets/audio/ambient_cave.wav");
    sound.RegisterBiomeSound(BiomeType::HiveGlade,      "src/Assets/audio/ambient_ocean.wav");

    sound.SetActiveBiome(BiomeType::MushroomForest);
}

void GameLayer::OnDetach() {
    SoundSystem::Get().StopBiomeSound();
}

void GameLayer::OnUpdate(float dt) {
    /// 1. Simulate physics and retrieve snapshots.
    const auto& snapshots = m_Physics->Step(dt);

    /// 2. Apply snapshots to Entity-Component system.
    m_World->ApplySnapshots(snapshots);

    /// 3. Update world logic
    m_World->Update(dt, *m_Network, m_Renderer);
}

void GameLayer::OnRender(Renderer* renderer) {
    if (m_PostProcessor) m_PostProcessor->BeginFrame(renderer);

    // World render
    m_World->Render(renderer, *m_Network);

    if (m_PostProcessor) m_PostProcessor->EndFrame(renderer);
}

void GameLayer::OnImGuiRender(Renderer* renderer) {
    if (m_PostProcessor) m_PostProcessor->OnImGui();
    m_World->OnImGuiRender(m_Timer->GetDeltaTime(), *m_Network, renderer);
    WorldDebugUI::Draw(*m_World);
}

void GameLayer::OnEvent(Event& event) {
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<EntityDamagedEvent>([this](EntityDamagedEvent& e) { return OnEntityDamaged(e); });
    dispatcher.Dispatch<EntityDiedEvent>([this](EntityDiedEvent& e) { return OnEntityDied(e); });
    dispatcher.Dispatch<BiomeChangedEvent>([this](BiomeChangedEvent& e) { return OnBiomeChanged(e); });
}

bool GameLayer::OnEntityDamaged(EntityDamagedEvent& e) {
    auto& sound = SoundSystem::Get();
    if (e.IsCritical()) {
        sound.PlaySound("assets/audio/sfx_hit_critical.wav", 1.0f, 1.2f);
    } else {
        float pitch = 0.9f + (static_cast<float>(rand() % 20) / 100.f);
        sound.PlaySound("assets/audio/sfx_hit_normal.wav", 0.8f, pitch);
    }
    return false;
}

bool GameLayer::OnEntityDied(EntityDiedEvent& /*e*/) {
    SoundSystem::Get().PlaySound("assets/audio/sfx_death.wav", 1.0f, 1.0f);
    return false;
}

bool GameLayer::OnBiomeChanged(BiomeChangedEvent& e) {
    SoundSystem::Get().SetActiveBiome(e.GetBiome());
    return false;
}
