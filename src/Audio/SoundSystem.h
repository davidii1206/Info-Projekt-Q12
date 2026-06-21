/**
 * @file SoundSystem.h
 * @brief OpenAL-based audio engine: 2D/3D playback, categories, ducking,
 *        fades and a biome ambient/playlist system.
 */

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

using SoundBuffer = ALuint; ///< Handle to a loaded OpenAL buffer (decoded audio data).
using SoundSource = ALuint; ///< Handle to an OpenAL source (a playing/playable voice).

// ── 3D Vektor (kompatibel mit glm::vec3 – einfach casten) ────────────────────

/**
 * @struct SoundVec3
 * @brief Minimal 3D vector, layout-compatible with glm::vec3 (cast directly).
 */
struct SoundVec3 {
    float x = 0.f; ///< X component.
    float y = 0.f; ///< Y component.
    float z = 0.f; ///< Z component.
};

#include "../Core/Biome.h"

// ── Biome-Typen ───────────────────────────────────────────────────────────────
// (BiomeType now defined in Biome.h)

// ── Lautstärke-Kategorien ─────────────────────────────────────────────────────

/**
 * @enum SoundCategory
 * @brief Mixer categories; each has its own volume multiplied into the master.
 */
enum class SoundCategory {
    Master,  ///< Global master bus.
    Effects, ///< Gameplay sound effects.
    Ambient, ///< Ambient / biome background loops.
    UI,      ///< User-interface sounds.
    Music    ///< Music tracks.
};

// ── Priorität (höhere Prio verdrängt niedrigere wenn Pool voll) ───────────────

/**
 * @enum SoundPriority
 * @brief Voice-stealing priority; higher priority evicts lower when the pool is full.
 */
enum class SoundPriority {
    Low    = 0, ///< Lowest priority, first to be evicted.
    Normal = 1, ///< Default priority.
    High   = 2, ///< High priority.
    Critical = 3   ///< Never discarded (e.g. player death).
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

/**
 * @struct PlaySoundParams
 * @brief Full set of options controlling a single Play() call.
 */
struct PlaySoundParams {
    // Basis
    float         volume   = 1.0f; ///< Base gain (0..1).
    float         pitch    = 1.0f; ///< Fixed pitch multiplier (used if pitchMin==pitchMax).
    float         pitchMin = 1.0f; ///< Lower bound for random pitch variation.
    float         pitchMax = 1.0f; ///< Upper bound for random pitch variation.
    bool          loop     = false; ///< Whether the sound loops.
    SoundCategory category = SoundCategory::Effects; ///< Mixer category.
    SoundPriority priority = SoundPriority::Normal;   ///< Voice-stealing priority.

    // 3D Positional Audio
    bool      is3D        = false; ///< Whether the source is positional (3D).
    SoundVec3 position    = {};    ///< World position (if is3D).
    SoundVec3 velocity    = {};    ///< Velocity, used for the Doppler effect.
    float     minDistance = 1.f;   ///< Distance at which attenuation begins.
    float     maxDistance = 50.f;  ///< Distance at which the sound becomes inaudible.
    float     rolloffFactor = 1.0f; ///< Attenuation rolloff factor.

    // Fade
    float fadeInSeconds  = 0.f;    ///< Fade-in time in seconds (0 = none).
    float fadeOutSeconds = 0.f;    ///< Fade-out time in seconds (0 = none; only useful with loop).

    // Cooldown: derselbe Sound wird innerhalb dieser Zeit nicht nochmal gespielt
    float cooldownSeconds = 0.f;   ///< Minimum seconds between replays of the same file.

    // Gruppe (z.B. "footsteps") – nur N gleichzeitig aktiv
    std::string group     = "";    ///< Group name; only maxInGroup play simultaneously.
    int         maxInGroup = 4;    ///< Max concurrent sources sharing this group.
};

// ── Aktive Source mit Metadaten ───────────────────────────────────────────────

/**
 * @struct ActiveSource
 * @brief A currently-allocated source plus bookkeeping for fades and ducking.
 */
struct ActiveSource {
    SoundSource   handle    = 0;   ///< Underlying OpenAL source handle.
    SoundPriority priority  = SoundPriority::Normal; ///< Priority for voice stealing.
    SoundCategory category  = SoundCategory::Effects; ///< Mixer category.
    std::string   group     = "";  ///< Group name (for per-group limits).
    float         baseVolume = 1.0f; ///< Unmodulated base gain.

    // Fade
    float fadeTarget   = 1.f;    ///< Target gain (0 = fade out, 1 = fade in).
    float fadeDuration = 0.f;    ///< Total fade duration in seconds.
    float fadeElapsed  = 0.f;    ///< Elapsed fade time in seconds.
    bool  stopAfterFade = false; ///< Whether to stop the source once the fade completes.

    // Ducking: temporär gedämpfter Gain-Multiplier
    float duckMultiplier = 1.f;  ///< Temporary ducking gain multiplier.
};

// ── Biome-Playlist ────────────────────────────────────────────────────────────

/**
 * @struct BiomePlaylist
 * @brief An ordered (optionally shuffled) set of ambient tracks for a biome.
 */
struct BiomePlaylist {
    std::vector<std::string> tracks; ///< Track file paths.
    size_t  currentIndex   = 0;      ///< Index of the currently playing track.
    bool    shuffle        = false;  ///< Whether to pick tracks randomly.
    float   crossfadeTime  = 2.0f;   ///< Crossfade duration in seconds.
};

// ── SoundSystem ───────────────────────────────────────────────────────────────

/**
 * @class SoundSystem
 * @brief Singleton OpenAL audio engine.
 *
 * Manages buffer loading, a fixed pool of sources with priority-based voice
 * stealing, per-category volumes, ducking, fades, 3D listener state and a
 * biome-driven ambient/playlist system. Call Update() once per frame.
 */
class SoundSystem {
public:
    /// @return The global SoundSystem singleton.
    static SoundSystem& Get();

    /// @brief Opens the OpenAL device/context and allocates the source pool.
    /// @return True on success.
    bool Init();
    /// @brief Stops all audio and releases the OpenAL device/context.
    void Shutdown();

    // ── Buffer-Verwaltung ────────────────────────────────────────────────────
    /// @brief Loads (or returns cached) a sound file into a buffer.
    /// @param filepath Path to the audio file (WAV).
    /// @return The buffer handle (0 on failure).
    SoundBuffer LoadSound(const std::string& filepath);
    /// @brief Frees the buffer associated with the given file.
    void        UnloadSound(const std::string& filepath);
    /// @brief Frees all loaded buffers.
    void        UnloadAll();

    // ── Playback ─────────────────────────────────────────────────────────────

    /**
     * @brief Convenience 2D playback.
     * @param filepath Audio file to play.
     * @param volume   Gain (0..1).
     * @param pitch    Pitch multiplier.
     * @param loop     Whether to loop.
     * @param category Mixer category.
     * @return The playing source handle (0 on failure).
     */
    SoundSource PlaySound(const std::string& filepath,
                          float volume   = 1.0f,
                          float pitch    = 1.0f,
                          bool  loop     = false,
                          SoundCategory category = SoundCategory::Effects);

    /**
     * @brief Full-control playback.
     * @param filepath Audio file to play.
     * @param params   Playback parameters.
     * @return The playing source handle (0 on failure).
     */
    SoundSource Play(const std::string& filepath, const PlaySoundParams& params = {});

    /**
     * @brief Plays a positional 3D sound at a world position.
     * @param filepath Audio file to play.
     * @param position World position.
     * @param volume   Gain (0..1).
     * @param maxDist  Distance at which the sound becomes inaudible.
     * @param category Mixer category.
     * @return The playing source handle (0 on failure).
     */
    SoundSource Play3D(const std::string& filepath,
                       SoundVec3 position,
                       float volume   = 1.0f,
                       float maxDist  = 50.f,
                       SoundCategory  category = SoundCategory::Effects);

    /**
     * @brief Plays a sound with randomised pitch (e.g. footsteps, hits).
     * @param filepath Audio file to play.
     * @param pitchMin Lower pitch bound.
     * @param pitchMax Upper pitch bound.
     * @param volume   Gain (0..1).
     * @param category Mixer category.
     * @return The playing source handle (0 on failure).
     */
    SoundSource PlayWithPitchVariation(const std::string& filepath,
                                       float pitchMin = 0.9f,
                                       float pitchMax = 1.1f,
                                       float volume   = 1.0f,
                                       SoundCategory  category = SoundCategory::Effects);

    /**
     * @brief Plays one randomly-chosen file from a list.
     * @param filepaths Candidate audio files.
     * @param params    Playback parameters.
     * @return The playing source handle (0 on failure).
     */
    SoundSource PlayRandom(const std::vector<std::string>& filepaths,
                           const PlaySoundParams& params = {});

    /// @brief Immediately stops a source.
    void StopSound(SoundSource source);
    /// @brief Fades a source out over @p seconds, then stops it.
    void FadeOut(SoundSource source, float seconds);
    /// @brief Fades a source in over @p seconds.
    void FadeIn(SoundSource source, float seconds);
    /// @brief Pauses a source.
    void PauseSound(SoundSource source);
    /// @brief Resumes a paused source.
    void ResumeSound(SoundSource source);

    /// @brief Sets a source's base gain.
    void SetSourceVolume(SoundSource source, float volume);
    /// @brief Sets a source's pitch multiplier.
    void SetSourcePitch(SoundSource source, float pitch);
    /// @brief Sets a 3D source's world position.
    void SetSourcePosition(SoundSource source, SoundVec3 pos);
    /// @brief Sets a 3D source's velocity (Doppler).
    void SetSourceVelocity(SoundSource source, SoundVec3 vel);

    /// @return True if the source is currently playing.
    bool IsPlaying(SoundSource source) const;

    // ── Listener (Kamera / Spieler) ──────────────────────────────────────────
    // Einmal pro Frame mit der Kamera-/Spielerposition aufrufen.
    // forward und up müssen normalisiert sein.
    /// @brief Sets the listener (camera/player) world position.
    void SetListenerPosition(SoundVec3 pos);
    /// @brief Sets the listener velocity (Doppler).
    void SetListenerVelocity(SoundVec3 vel);
    /// @brief Sets the listener orientation; @p forward and @p up must be normalised.
    void SetListenerOrientation(SoundVec3 forward, SoundVec3 up);

    // ── Biome-Ambient-System ─────────────────────────────────────────────────
    /// @brief Registers a single ambient file for a biome.
    void RegisterBiomeSound(BiomeType biome, const std::string& filepath);
    /// @brief Registers a multi-track playlist for a biome.
    void RegisterBiomePlaylist(BiomeType biome, BiomePlaylist playlist);
    /// @brief Switches the active biome, crossfading its ambient audio.
    void SetActiveBiome(BiomeType biome);
    /// @brief Stops the currently playing biome ambient.
    void StopBiomeSound();
    /// @brief Sets the biome ambient base volume.
    void SetBiomeVolume(float volume);
    /// @return The currently active biome.
    BiomeType GetCurrentBiome() const { return m_CurrentBiome; }

    // ── Lautstärke-Kategorien ────────────────────────────────────────────────
    /// @brief Sets the volume of a mixer category.
    void  SetCategoryVolume(SoundCategory category, float volume);
    /// @return The volume of a mixer category.
    float GetCategoryVolume(SoundCategory category) const;
    /// @brief Sets the master volume.
    void  SetMasterVolume(float volume);
    /// @return The master volume.
    float GetMasterVolume() const { return m_VolumeMaster; }

    // ── Ducking ──────────────────────────────────────────────────────────────
    /**
     * @brief Temporarily attenuates all sources of a category.
     * @param category Category to duck.
     * @param factor   Gain multiplier while ducked (e.g. 0.3).
     * @param duration Duck duration in seconds.
     */
    void DuckCategory(SoundCategory category, float factor, float duration);

    // ── Globale Steuerung ────────────────────────────────────────────────────
    /// @brief Pauses every active source.
    void PauseAll();
    /// @brief Resumes every paused source.
    void ResumeAll();
    /// @brief Stops every active source.
    void StopAll();

    // ── Update (einmal pro Frame) ─────────────────────────────────────────────
    /// @brief Advances fades, ducking, crossfades and cleanup. Call once per frame.
    /// @param deltaTime Seconds since the last frame.
    void Update(float deltaTime);

private:
    SoundSystem();                                       ///< Private ctor (singleton).
    ~SoundSystem() = default;                            ///< Default dtor.
    SoundSystem(const SoundSystem&)            = delete; ///< Non-copyable.
    SoundSystem& operator=(const SoundSystem&) = delete; ///< Non-assignable.

    /// @brief Loads and decodes a WAV file into an OpenAL buffer.
    ALuint      LoadWAV(const std::string& filepath);
    /// @brief Acquires a free source, stealing a lower-priority one if needed.
    SoundSource AcquireSource(SoundPriority priority);

    /// @brief Computes the final gain for a base volume in a category (incl. master/duck).
    float EffectiveVolume(float base, SoundCategory category) const;
    /// @brief Returns a random float in [min, max].
    float RandomFloat(float min, float max);

    /// @brief Applies PlaySoundParams (3D, distance, loop, pitch) to a source.
    void ApplySourceParams(SoundSource src, const PlaySoundParams& p, float resolvedPitch);
    /// @brief Advances active fades by @p dt.
    void UpdateFades(float dt);
    /// @brief Advances the biome ambient crossfade by @p dt.
    void UpdateBiomeCrossfade(float dt);
    /// @brief Removes finished sources from the active pool.
    void CleanupStoppedSources();

    // ── State ────────────────────────────────────────────────────────────────
    ALCdevice*  m_Device  = nullptr; ///< OpenAL device.
    ALCcontext* m_Context = nullptr; ///< OpenAL context.

    std::unordered_map<std::string, ALuint>        m_Buffers;        ///< Cache: filepath → buffer.
    std::unordered_map<BiomeType, std::string>     m_BiomeSounds;    ///< Single ambient per biome.
    std::unordered_map<BiomeType, BiomePlaylist>   m_BiomePlaylists; ///< Playlists per biome.
    std::vector<ActiveSource>                      m_ActiveSources;  ///< Currently allocated sources.

    // Cooldown: filepath → letzter Abspielzeitpunkt
    std::unordered_map<std::string, float>         m_CooldownTimers; ///< filepath → last play time.
    float m_GlobalTime = 0.f; ///< Accumulated time (for cooldowns).

    // Biome
    SoundSource m_BiomeSource       = 0;   ///< Current biome ambient source.
    SoundSource m_BiomeFadeSource   = 0;   ///< Outgoing biome source being faded out.
    float       m_BiomeFadeTimer    = 0.f; ///< Elapsed biome crossfade time.
    float       m_BiomeFadeDuration = 0.f; ///< Total biome crossfade duration.
    BiomeType   m_CurrentBiome      = BiomeType::None; ///< Active biome.
    float       m_BiomeBaseVolume   = 1.0f; ///< Biome ambient base volume.

    /// @struct DuckState
    /// @brief Per-category active ducking state.
    struct DuckState {
        float factor    = 1.f; ///< Current gain multiplier.
        float remaining = 0.f; ///< Remaining duck time in seconds.
    };
    std::unordered_map<int, DuckState> m_DuckStates; ///< Ducking state keyed by category.

    // Volume pro Kategorie
    float m_VolumeMaster  = 1.0f; ///< Master volume.
    float m_VolumeEffects = 1.0f; ///< Effects category volume.
    float m_VolumeAmbient = 1.0f; ///< Ambient category volume.
    float m_VolumeUI      = 1.0f; ///< UI category volume.
    float m_VolumeMusic   = 1.0f; ///< Music category volume.

    bool m_Initialized = false; ///< Whether Init() succeeded.

    std::mt19937                          m_Rng;            ///< RNG for pitch/track randomisation.
    std::uniform_real_distribution<float> m_Dist{ 0.f, 1.f }; ///< Uniform [0,1] distribution.

    static constexpr int   kMAX_SOURCES    = 64;  ///< Size of the source pool.
    static constexpr float kCROSSFADE_TIME = 2.0f; ///< Default crossfade time in seconds.
};
