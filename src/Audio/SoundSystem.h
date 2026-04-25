#pragma once
#include <AL/al.h>
#include <AL/alc.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <functional>
#include <optional>
#include <chrono>

// Windows definiert PlaySound als Makro (→ PlaySoundA/W) – das kollidiert
// mit SoundSystem::PlaySound. Hier sauber entfernen.
#ifdef PlaySound
#  undef PlaySound
#endif

// ── Sound Handles ─────────────────────────────────────────────────────────────

using SoundBuffer = ALuint;
using SoundSource = ALuint;

// ── 3D Vektor (kompatibel mit glm::vec3 – einfach casten) ────────────────────

struct SoundVec3 {
    float x = 0.f, y = 0.f, z = 0.f;
};

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

enum class SoundCategory {
    Master,
    Effects,
    Ambient,
    UI,
    Music
};

// ── Priorität (höhere Prio verdrängt niedrigere wenn Pool voll) ───────────────

enum class SoundPriority {
    Low    = 0,
    Normal = 1,
    High   = 2,
    Critical = 3   // Wird nie verworfen (z.B. Spielertod)
};

// ── PlaySoundParams ───────────────────────────────────────────────────────────
//
// Beispiel – positionaler 3D-Sound mit Pitch-Variation:
//   PlaySoundParams p;
//   p.position  = { entity.x, entity.y, entity.z };
//   p.is3D      = true;
//   p.pitchMin  = 0.9f;
//   p.pitchMax  = 1.1f;
//   p.maxDistance  = 50.f;
//   SoundSystem::Get().Play("assets/audio/sfx_hit.wav", p);

struct PlaySoundParams {
    // Basis
    float         volume   = 1.0f;
    float         pitch    = 1.0f;
    float         pitchMin = 1.0f;
    float         pitchMax = 1.0f;
    bool          loop     = false;
    SoundCategory category = SoundCategory::Effects;
    SoundPriority priority = SoundPriority::Normal;

    // 3D Positional Audio
    bool      is3D        = false;
    SoundVec3 position    = {};
    SoundVec3 velocity    = {};       // Für Doppler
    float     minDistance = 1.f;      // Ab hier wird's leiser
    float     maxDistance = 50.f;     // Ab hier unhörbar
    float     rolloffFactor = 1.0f;

    // Fade
    float fadeInSeconds  = 0.f;       // 0 = kein Fade-In
    float fadeOutSeconds = 0.f;       // 0 = kein Fade-Out (nur bei loop=true sinnvoll)

    // Cooldown: derselbe Sound wird innerhalb dieser Zeit nicht nochmal gespielt
    float cooldownSeconds = 0.f;

    // Gruppe (z.B. "footsteps") – nur N gleichzeitig aktiv
    std::string group     = "";
    int         maxInGroup = 4;
};

// ── Aktive Source mit Metadaten ───────────────────────────────────────────────

struct ActiveSource {
    SoundSource   handle    = 0;
    SoundPriority priority  = SoundPriority::Normal;
    SoundCategory category  = SoundCategory::Effects;
    std::string   group     = "";
    float         baseVolume = 1.0f;

    // Fade
    float fadeTarget   = 1.f;    // Ziel-Gain (0=ausblenden, 1=einblenden)
    float fadeDuration = 0.f;
    float fadeElapsed  = 0.f;
    bool  stopAfterFade = false;

    // Ducking: temporär gedämpfter Gain-Multiplier
    float duckMultiplier = 1.f;
};

// ── Biome-Playlist ────────────────────────────────────────────────────────────

struct BiomePlaylist {
    std::vector<std::string> tracks;
    size_t  currentIndex   = 0;
    bool    shuffle        = false;
    float   crossfadeTime  = 2.0f;   // Sekunden Überblendzeit
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

    // Kurzform (2D)
    SoundSource PlaySound(const std::string& filepath,
                          float volume   = 1.0f,
                          float pitch    = 1.0f,
                          bool  loop     = false,
                          SoundCategory category = SoundCategory::Effects);

    // Vollform mit PlaySoundParams
    SoundSource Play(const std::string& filepath, const PlaySoundParams& params = {});

    // 3D-Sound an Weltposition
    SoundSource Play3D(const std::string& filepath,
                       SoundVec3 position,
                       float volume   = 1.0f,
                       float maxDist  = 50.f,
                       SoundCategory  category = SoundCategory::Effects);

    // Pitch-Variation (z.B. Schritte, Treffer)
    SoundSource PlayWithPitchVariation(const std::string& filepath,
                                       float pitchMin = 0.9f,
                                       float pitchMax = 1.1f,
                                       float volume   = 1.0f,
                                       SoundCategory  category = SoundCategory::Effects);

    // Zufällig aus einer Liste (z.B. mehrere Treffergeräusche)
    SoundSource PlayRandom(const std::vector<std::string>& filepaths,
                           const PlaySoundParams& params = {});

    void StopSound(SoundSource source);
    void FadeOut(SoundSource source, float seconds);
    void FadeIn(SoundSource source, float seconds);
    void PauseSound(SoundSource source);
    void ResumeSound(SoundSource source);

    void SetSourceVolume(SoundSource source, float volume);
    void SetSourcePitch(SoundSource source, float pitch);
    void SetSourcePosition(SoundSource source, SoundVec3 pos);
    void SetSourceVelocity(SoundSource source, SoundVec3 vel);

    bool IsPlaying(SoundSource source) const;

    // ── Listener (Kamera / Spieler) ──────────────────────────────────────────
    // Einmal pro Frame mit der Kamera-/Spielerposition aufrufen.
    // forward und up müssen normalisiert sein.
    void SetListenerPosition(SoundVec3 pos);
    void SetListenerVelocity(SoundVec3 vel);
    void SetListenerOrientation(SoundVec3 forward, SoundVec3 up);

    // ── Biome-Ambient-System ─────────────────────────────────────────────────
    void RegisterBiomeSound(BiomeType biome, const std::string& filepath);
    void RegisterBiomePlaylist(BiomeType biome, BiomePlaylist playlist);
    void SetActiveBiome(BiomeType biome);
    void StopBiomeSound();
    void SetBiomeVolume(float volume);
    BiomeType GetCurrentBiome() const { return m_CurrentBiome; }

    // ── Lautstärke-Kategorien ────────────────────────────────────────────────
    void  SetCategoryVolume(SoundCategory category, float volume);
    float GetCategoryVolume(SoundCategory category) const;
    void  SetMasterVolume(float volume);
    float GetMasterVolume() const { return m_VolumeMaster; }

    // ── Ducking ──────────────────────────────────────────────────────────────
    // Alle Sources der Kategorie werden für 'duration' Sekunden auf 'factor'
    // abgesenkt (z.B. Musik leiser wenn Cutscene startet).
    void DuckCategory(SoundCategory category, float factor, float duration);

    // ── Globale Steuerung ────────────────────────────────────────────────────
    void PauseAll();
    void ResumeAll();
    void StopAll();

    // ── Update (einmal pro Frame) ─────────────────────────────────────────────
    void Update(float deltaTime);

private:
    SoundSystem();
    ~SoundSystem() = default;
    SoundSystem(const SoundSystem&)            = delete;
    SoundSystem& operator=(const SoundSystem&) = delete;

    ALuint      LoadWAV(const std::string& filepath);
    SoundSource AcquireSource(SoundPriority priority);

    float EffectiveVolume(float base, SoundCategory category) const;
    float RandomFloat(float min, float max);

    void ApplySourceParams(SoundSource src, const PlaySoundParams& p, float resolvedPitch);
    void UpdateFades(float dt);
    void UpdateBiomeCrossfade(float dt);
    void CleanupStoppedSources();

    // ── State ────────────────────────────────────────────────────────────────
    ALCdevice*  m_Device  = nullptr;
    ALCcontext* m_Context = nullptr;

    std::unordered_map<std::string, ALuint>        m_Buffers;
    std::unordered_map<BiomeType, std::string>     m_BiomeSounds;
    std::unordered_map<BiomeType, BiomePlaylist>   m_BiomePlaylists;
    std::vector<ActiveSource>                      m_ActiveSources;

    // Cooldown: filepath → letzter Abspielzeitpunkt
    std::unordered_map<std::string, float>         m_CooldownTimers;
    float m_GlobalTime = 0.f;

    // Biome
    SoundSource m_BiomeSource       = 0;
    SoundSource m_BiomeFadeSource   = 0;   // Ausblendendes Biome-Audio
    float       m_BiomeFadeTimer    = 0.f;
    float       m_BiomeFadeDuration = 0.f;
    BiomeType   m_CurrentBiome      = BiomeType::None;
    float       m_BiomeBaseVolume   = 1.0f;

    // Ducking pro Kategorie
    struct DuckState {
        float factor    = 1.f;
        float remaining = 0.f;
    };
    std::unordered_map<int, DuckState> m_DuckStates;

    // Volume pro Kategorie
    float m_VolumeMaster  = 1.0f;
    float m_VolumeEffects = 1.0f;
    float m_VolumeAmbient = 1.0f;
    float m_VolumeUI      = 1.0f;
    float m_VolumeMusic   = 1.0f;

    bool m_Initialized = false;

    std::mt19937                          m_Rng;
    std::uniform_real_distribution<float> m_Dist{ 0.f, 1.f };

    static constexpr int   kMAX_SOURCES    = 64;
    static constexpr float kCROSSFADE_TIME = 2.0f;
};