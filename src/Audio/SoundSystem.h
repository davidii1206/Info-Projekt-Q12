#pragma once
#include <AL/al.h>
#include <AL/alc.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>

// ── Sound Handles ─────────────────────────────────────────────────────────────

using SoundBuffer = ALuint;
using SoundSource = ALuint;

// ── Biome-Typen ───────────────────────────────────────────────────────────────

enum class BiomeType {
    None,
    Forest,
    Desert,
    Cave,
    Ocean,
    Tundra
};

// ── Lautstärke-Kategorien ─────────────────────────────────────────────────────
// Jede Kategorie hat ein eigenes Volume das unabhängig vom Master skaliert wird.
// Nützlich für "Effekte leiser, Musik lauter" Einstellungen im Options-Menü.

enum class SoundCategory {
    Master,   // Globaler Multiplikator (wirkt auf alle anderen)
    Effects,  // Kampf, Schritte, Umgebungs-SFX,
    Ambient,  // Biome-Loops
    UI        // Menü-Klicks, Inventar,
};

// ── PlaySoundParams ───────────────────────────────────────────────────────────
// Komfort-Struct – nur die Felder setzen die du brauchst, der Rest hat Defaults.
//
// Beispiel mit Pitch-Variation:
//   PlaySoundParams p;
//   p.volume   = 0.8f;
//   p.pitchMin = 0.9f;
//   p.pitchMax = 1.1f;
//   SoundSystem::Get().Play("assets/audio/sfx_hit.wav", p);

struct PlaySoundParams {
    float volume   = 1.0f;   // Basis-Lautstärke (0.0 – 1.0)
    float pitch    = 1.0f;   // Basis-Pitch (wird ignoriert wenn pitchMin != pitchMax)
    float pitchMin = 1.0f;   // Zufälliger Pitch-Bereich untere Grenze
    float pitchMax = 1.0f;   // Zufälliger Pitch-Bereich obere Grenze
    bool  loop     = false;
    SoundCategory category = SoundCategory::Effects;
};

// ── SoundSystem ───────────────────────────────────────────────────────────────

class SoundSystem {
public:
    static SoundSystem& Get();

    bool Init();
    void Shutdown();

    // ── Buffer-Verwaltung ────────────────────────────────────────────────────
    SoundBuffer LoadSound(const std::string& filepath);
    void        UnloadSound(const std::string& filepath);
    void        UnloadAll();

    // ── Playback ─────────────────────────────────────────────────────────────

    // Kurzform – kompatibel mit dem alten API
    SoundSource PlaySound(const std::string& filepath,
                          float volume   = 1.0f,
                          float pitch    = 1.0f,
                          bool  loop     = false,
                          SoundCategory category = SoundCategory::Effects);

    // Vollform mit PlaySoundParams (inkl. Pitch-Variation)
    SoundSource Play(const std::string& filepath, const PlaySoundParams& params = {});

    // Shortcut: Pitch wird zufällig zwischen pitchMin und pitchMax gewählt.
    // Ideal für Treffergeräusche, Schritte, Explosionen – klingt nie monoton.
    SoundSource PlayWithPitchVariation(const std::string& filepath,
                                       float pitchMin = 0.9f,
                                       float pitchMax = 1.1f,
                                       float volume   = 1.0f,
                                       SoundCategory category = SoundCategory::Effects);

    void StopSound(SoundSource source);
    void PauseSound(SoundSource source);
    void ResumeSound(SoundSource source);

    // Lautstärke / Pitch einer laufenden Source nachträglich ändern
    void SetSourceVolume(SoundSource source, float volume);
    void SetSourcePitch(SoundSource source, float pitch);

    // Gibt true zurück solange die Source noch abspielt
    bool IsPlaying(SoundSource source) const;

    // ── Biome-Ambient-System ─────────────────────────────────────────────────
    void RegisterBiomeSound(BiomeType biome, const std::string& filepath);
    void SetActiveBiome(BiomeType biome);
    void StopBiomeSound();
    // Lautstärke des laufenden Biome-Loops (unabhängig von Ambient-Kategorie)
    void SetBiomeVolume(float volume);
    BiomeType GetCurrentBiome() const { return m_CurrentBiome; }

    // ── Lautstärke-Kategorien ────────────────────────────────────────────────
    void  SetCategoryVolume(SoundCategory category, float volume); // 0.0 – 1.0
    float GetCategoryVolume(SoundCategory category) const;

    // Master-Shortcuts
    void  SetMasterVolume(float volume);
    float GetMasterVolume() const { return m_VolumeMaster; }

    // ── Globale Steuerung ────────────────────────────────────────────────────
    void PauseAll();
    void ResumeAll();
    void StopAll();  // Nur One-Shot-Sources; Biome-Loop läuft weiter

    // ── Update (einmal pro Frame in GameLayer::OnUpdate) ─────────────────────
    void Update();

private:
    SoundSystem();
    ~SoundSystem() = default;
    SoundSystem(const SoundSystem&)            = delete;
    SoundSystem& operator=(const SoundSystem&) = delete;

    ALuint      LoadWAV(const std::string& filepath);
    SoundSource AcquireSource();

    // Effektive Lautstärke = base * categoryVolume * masterVolume
    float EffectiveVolume(float base, SoundCategory category) const;
    float RandomFloat(float min, float max);

    // ── State ────────────────────────────────────────────────────────────────
    ALCdevice*  m_Device  = nullptr;
    ALCcontext* m_Context = nullptr;

    std::unordered_map<std::string, ALuint>    m_Buffers;
    std::unordered_map<BiomeType, std::string> m_BiomeSounds;
    std::vector<SoundSource>                   m_ActiveSources;

    SoundSource m_BiomeSource     = 0;
    BiomeType   m_CurrentBiome    = BiomeType::None;
    float       m_BiomeBaseVolume = 1.0f;

    float m_VolumeMaster  = 1.0f;
    float m_VolumeEffects = 1.0f;
    float m_VolumeAmbient = 1.0f;
    float m_VolumeUI      = 1.0f;

    bool m_Initialized = false;

    std::mt19937                          m_Rng;
    std::uniform_real_distribution<float> m_Dist{ 0.f, 1.f };

    static constexpr int kMAX_SOURCES = 32;
};
