#pragma once
#include "../Core/Layer.h"
#include "../Audio/SoundSystem.h"
#include "../Audio/SoundEvents.h"
#include "World.h"
#include <memory>

// GameLayer – erweitert um Sound-System-Integration
// Dieses Beispiel zeigt, wie Biome-Sounds und Event-Sounds verdrahtet werden.
class GameLayer : public Layer {
public:
    GameLayer() : Layer("GameLayer") {}

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    void OnAttach() override {
        m_World = std::make_unique<World>();

        // Biome-Sounds registrieren (Pfade relativ zum Arbeitsverzeichnis)
        auto& sound = SoundSystem::Get();
        sound.RegisterBiomeSound(BiomeType::Forest, "assets/audio/ambient_forest.wav");
        sound.RegisterBiomeSound(BiomeType::Desert, "assets/audio/ambient_desert.wav");
        sound.RegisterBiomeSound(BiomeType::Cave,   "assets/audio/ambient_cave.wav");
        sound.RegisterBiomeSound(BiomeType::Ocean,  "assets/audio/ambient_ocean.wav");
        sound.RegisterBiomeSound(BiomeType::Tundra, "assets/audio/ambient_tundra.wav");

        // Starte mit Startbiom
        sound.SetActiveBiome(BiomeType::Forest);
    }

    void OnDetach() override {
        SoundSystem::Get().StopBiomeSound();
        m_World.reset();
    }

    void OnUpdate(float dt) override {
        // Sound-System jeden Frame aktualisieren (räumt beendete Sources auf)
        SoundSystem::Get().Update(dt);

        if (m_World)
            m_World->Update(dt);
    }

    // ── Event-Handling ────────────────────────────────────────────────────────

    void OnEvent(Event& event) override {
        EventDispatcher dispatcher(event);

        // Sound-relevante Events abfangen
        dispatcher.Dispatch<EntityDamagedEvent>(
            [this](EntityDamagedEvent& e) { return OnEntityDamaged(e); });

        dispatcher.Dispatch<EntityDiedEvent>(
            [this](EntityDiedEvent& e) { return OnEntityDied(e); });

        dispatcher.Dispatch<BiomeChangedEvent>(
            [this](BiomeChangedEvent& e) { return OnBiomeChanged(e); });
    }

    World* GetWorld() { return m_World.get(); }

    // ── Hilfsmethoden (von außen aufrufbar) ───────────────────────────────────

    // Rufe das aus deiner Spiellogik auf wenn der Spieler in ein neues Biom tritt
    void ChangeBiome(BiomeType biome) {
        BiomeChangedEvent e(biome);
        OnEvent(e);
    }

    // Einfaches Auslösen eines Schadens-Sounds (z.B. aus einem Combat-System)
    void TriggerDamageSound(float amount, bool critical = false) {
        EntityDamagedEvent e(amount, critical);
        OnEvent(e);
    }

private:
    // ── Sound-Event-Handler ───────────────────────────────────────────────────

    bool OnEntityDamaged(EntityDamagedEvent& e) {
        auto& sound = SoundSystem::Get();
        if (e.IsCritical()) {
            sound.PlaySound("assets/audio/sfx_hit_critical.wav", 1.0f, 1.2f);
        } else {
            // Pitch leicht variieren für Abwechslung
            float pitch = 0.9f + (static_cast<float>(rand() % 20) / 100.f);
            sound.PlaySound("assets/audio/sfx_hit_normal.wav", 0.8f, pitch);
        }
        return false; // Event nicht konsumieren – andere Layer sollen es auch erhalten
    }

    bool OnEntityDied(EntityDiedEvent& /*e*/) {
        SoundSystem::Get().PlaySound("assets/audio/sfx_death.wav", 1.0f, 1.0f);
        return false;
    }

    bool OnBiomeChanged(BiomeChangedEvent& e) {
        SoundSystem::Get().SetActiveBiome(e.GetBiome());
        return false;
    }

    // ── State ─────────────────────────────────────────────────────────────────
    std::unique_ptr<World> m_World;
};
