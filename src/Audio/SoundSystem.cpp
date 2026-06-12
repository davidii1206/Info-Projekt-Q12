#include "SoundSystem.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cmath>

// ── Minimaler WAV-Parser ──────────────────────────────────────────────────────

struct WavHeader {
    char     riff[4];
    uint32_t chunkSize;
    char     wave[4];
    char     fmt[4];
    uint32_t fmtSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};

static bool LoadWAVFile(const std::string& path,
                        std::vector<uint8_t>& outData,
                        ALenum& outFormat,
                        ALsizei& outFreq)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        spdlog::error("[SoundSystem] Cannot open: {}", path);
        return false;
    }

    WavHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (std::strncmp(header.riff, "RIFF", 4) != 0 ||
        std::strncmp(header.wave, "WAVE", 4) != 0) {
        spdlog::error("[SoundSystem] Not a valid WAV file: {}", path);
        return false;
    }
    if (header.audioFormat != 1) {
        spdlog::error("[SoundSystem] Only PCM WAV supported: {}", path);
        return false;
    }

    // Springe zum "data"-Chunk (überspringt optionale Chunks)
    char     chunkId[4];
    uint32_t chunkSize = 0;
    while (file.read(chunkId, 4) && file.read(reinterpret_cast<char*>(&chunkSize), 4)) {
        if (std::strncmp(chunkId, "data", 4) == 0) break;
        file.seekg(chunkSize, std::ios::cur);
    }

    outData.resize(chunkSize);
    file.read(reinterpret_cast<char*>(outData.data()), chunkSize);
    outFreq = static_cast<ALsizei>(header.sampleRate);

    if (header.numChannels == 1)
        outFormat = (header.bitsPerSample == 8) ? AL_FORMAT_MONO8   : AL_FORMAT_MONO16;
    else
        outFormat = (header.bitsPerSample == 8) ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;

    return true;
}

// ── Singleton ─────────────────────────────────────────────────────────────────

SoundSystem::SoundSystem()
    : m_Rng(std::random_device{}()) {}

SoundSystem& SoundSystem::Get() {
    static SoundSystem instance;
    return instance;
}

// ── Init / Shutdown ───────────────────────────────────────────────────────────

bool SoundSystem::Init() {
    m_Device = alcOpenDevice(nullptr);
    if (!m_Device) {
        spdlog::error("[SoundSystem] Failed to open audio device");
        return false;
    }

    m_Context = alcCreateContext(m_Device, nullptr);
    if (!m_Context) {
        spdlog::error("[SoundSystem] Failed to create OpenAL context");
        alcCloseDevice(m_Device);
        m_Device = nullptr;
        return false;
    }

    alcMakeContextCurrent(m_Context);

    // Doppler und Distance-Model konfigurieren
    alDopplerFactor(1.0f);
    alDopplerVelocity(343.0f);   // Schallgeschwindigkeit in m/s
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

    // Listener-Defaults
    alListener3f(AL_POSITION, 0.f, 0.f, 0.f);
    alListener3f(AL_VELOCITY, 0.f, 0.f, 0.f);
    alListenerf(AL_GAIN, m_VolumeMaster);

    float orientation[6] = { 0.f, 0.f, -1.f,   // Forward
                              0.f, 1.f,  0.f };  // Up
    alListenerfv(AL_ORIENTATION, orientation);

    m_Initialized = true;
    spdlog::info("[SoundSystem] Initialized (OpenAL, max {} sources)", kMAX_SOURCES);
    return true;
}

void SoundSystem::Shutdown() {
    if (!m_Initialized) return;

    StopBiomeSound();

    for (auto& as : m_ActiveSources) {
        alSourceStop(as.handle);
        alDeleteSources(1, &as.handle);
    }
    m_ActiveSources.clear();

    UnloadAll();

    alcMakeContextCurrent(nullptr);
    if (m_Context) alcDestroyContext(m_Context);
    if (m_Device)  alcCloseDevice(m_Device);

    m_Context     = nullptr;
    m_Device      = nullptr;
    m_Initialized = false;

    spdlog::info("[SoundSystem] Shutdown");
}

// ── Hilfsfunktionen ───────────────────────────────────────────────────────────

float SoundSystem::EffectiveVolume(float base, SoundCategory category) const {
    float catVol = 1.0f;
    switch (category) {
        case SoundCategory::Effects: catVol = m_VolumeEffects; break;
        case SoundCategory::Ambient: catVol = m_VolumeAmbient; break;
        case SoundCategory::UI:      catVol = m_VolumeUI;      break;
        case SoundCategory::Music:   catVol = m_VolumeMusic;   break;
        default: break;
    }

    // Duck-Multiplikator für Kategorie anwenden
    float duck = 1.f;
    auto it = m_DuckStates.find(static_cast<int>(category));
    if (it != m_DuckStates.end() && it->second.remaining > 0.f)
        duck = it->second.factor;

    return base * catVol * m_VolumeMaster * duck;
}

float SoundSystem::RandomFloat(float min, float max) {
    return min + m_Dist(m_Rng) * (max - min);
}

// ── Buffer-Verwaltung ─────────────────────────────────────────────────────────

SoundBuffer SoundSystem::LoadSound(const std::string& filepath) {
    auto it = m_Buffers.find(filepath);
    if (it != m_Buffers.end()) return it->second;

    ALuint buffer = LoadWAV(filepath);
    if (buffer == 0) return 0;

    m_Buffers[filepath] = buffer;
    spdlog::debug("[SoundSystem] Loaded: {}", filepath);
    return buffer;
}

void SoundSystem::UnloadSound(const std::string& filepath) {
    auto it = m_Buffers.find(filepath);
    if (it == m_Buffers.end()) return;
    alDeleteBuffers(1, &it->second);
    m_Buffers.erase(it);
}

void SoundSystem::UnloadAll() {
    for (auto& [path, buf] : m_Buffers)
        alDeleteBuffers(1, &buf);
    m_Buffers.clear();
}

ALuint SoundSystem::LoadWAV(const std::string& filepath) {
    std::vector<uint8_t> data;
    ALenum  format;
    ALsizei freq;

    if (!LoadWAVFile(filepath, data, format, freq)) return 0;

    ALuint buffer;
    alGenBuffers(1, &buffer);
    alBufferData(buffer, format, data.data(), static_cast<ALsizei>(data.size()), freq);

    if (alGetError() != AL_NO_ERROR) {
        spdlog::error("[SoundSystem] alBufferData failed for: {}", filepath);
        alDeleteBuffers(1, &buffer);
        return 0;
    }
    return buffer;
}

// ── Source-Pool ───────────────────────────────────────────────────────────────

SoundSource SoundSystem::AcquireSource(SoundPriority priority) {
    // Cleanup fertig gespielter Sources
    CleanupStoppedSources();

    if (static_cast<int>(m_ActiveSources.size()) < kMAX_SOURCES) {
        ALuint src;
        alGenSources(1, &src);
        return src;
    }

    // Pool voll: Niedrig-Prio Source verdrängen
    if (priority == SoundPriority::Critical) {
        // Immer Platz schaffen
    }

    // Finde niedrigste Priorität zum Ersetzen
    auto lowest = std::min_element(m_ActiveSources.begin(), m_ActiveSources.end(),
        [](const ActiveSource& a, const ActiveSource& b) {
            return static_cast<int>(a.priority) < static_cast<int>(b.priority);
        });

    if (lowest != m_ActiveSources.end() &&
        static_cast<int>(lowest->priority) < static_cast<int>(priority))
    {
        SoundSource stolen = lowest->handle;
        alSourceStop(stolen);
        m_ActiveSources.erase(lowest);
        spdlog::debug("[SoundSystem] Preempted low-priority source");
        return stolen;
    }

    spdlog::warn("[SoundSystem] Source pool exhausted, sound dropped");
    return 0;
}

// ── ApplySourceParams ─────────────────────────────────────────────────────────

void SoundSystem::ApplySourceParams(SoundSource src, const PlaySoundParams& p, float resolvedPitch) {
    float gain = (p.fadeInSeconds > 0.f) ? 0.f : EffectiveVolume(p.volume, p.category);

    alSourcef (src, AL_PITCH,    resolvedPitch);
    alSourcef (src, AL_GAIN,     gain);
    alSourcei (src, AL_LOOPING,  p.loop ? AL_TRUE : AL_FALSE);

    if (p.is3D) {
        alSourcei (src, AL_SOURCE_RELATIVE,        AL_FALSE);
        alSource3f(src, AL_POSITION,               p.position.x, p.position.y, p.position.z);
        alSource3f(src, AL_VELOCITY,               p.velocity.x, p.velocity.y, p.velocity.z);
        alSourcef (src, AL_REFERENCE_DISTANCE,     p.minDistance);
        alSourcef (src, AL_MAX_DISTANCE,           p.maxDistance);
        alSourcef (src, AL_ROLLOFF_FACTOR,         p.rolloffFactor);
    } else {
        // 2D: relativ zum Listener, keine Positionierung
        alSourcei (src, AL_SOURCE_RELATIVE, AL_TRUE);
        alSource3f(src, AL_POSITION,        0.f, 0.f, 0.f);
        alSourcef (src, AL_ROLLOFF_FACTOR,  0.f);
    }
}

// ── Playback ──────────────────────────────────────────────────────────────────

SoundSource SoundSystem::PlaySound(const std::string& filepath,
                                   float volume, float pitch, bool loop,
                                   SoundCategory category)
{
    PlaySoundParams p;
    p.volume = volume;
    p.pitch  = pitch;
    p.pitchMin = pitch;
    p.pitchMax = pitch;
    p.loop   = loop;
    p.category = category;
    return Play(filepath, p);
}

SoundSource SoundSystem::Play(const std::string& filepath, const PlaySoundParams& params) {
    if (!m_Initialized) return 0;

    // Cooldown-Check
    if (params.cooldownSeconds > 0.f) {
        auto it = m_CooldownTimers.find(filepath);
        if (it != m_CooldownTimers.end() &&
            (m_GlobalTime - it->second) < params.cooldownSeconds) {
            return 0; // Noch im Cooldown
        }
        m_CooldownTimers[filepath] = m_GlobalTime;
    }

    // Gruppen-Limit
    if (!params.group.empty()) {
        int count = 0;
        for (const auto& as : m_ActiveSources)
            if (as.group == params.group) ++count;

        if (count >= params.maxInGroup) {
            // Älteste Source der Gruppe ersetzen
            for (auto it2 = m_ActiveSources.begin(); it2 != m_ActiveSources.end(); ++it2) {
                if (it2->group == params.group) {
                    alSourceStop(it2->handle);
                    alDeleteSources(1, &it2->handle);
                    m_ActiveSources.erase(it2);
                    break;
                }
            }
        }
    }

    ALuint buffer = LoadSound(filepath);
    if (buffer == 0) return 0;

    SoundSource src = AcquireSource(params.priority);
    if (src == 0) return 0;

    float pitch = (params.pitchMin != params.pitchMax)
                ? RandomFloat(params.pitchMin, params.pitchMax)
                : params.pitch;

    alSourcei(src, AL_BUFFER, static_cast<ALint>(buffer));
    ApplySourceParams(src, params, pitch);
    alSourcePlay(src);

    // Metadaten speichern
    ActiveSource as;
    as.handle     = src;
    as.priority   = params.priority;
    as.category   = params.category;
    as.group      = params.group;
    as.baseVolume = params.volume;

    // Fade-In einrichten
    if (params.fadeInSeconds > 0.f) {
        as.fadeTarget   = 1.f;
        as.fadeDuration = params.fadeInSeconds;
        as.fadeElapsed  = 0.f;
    }

    m_ActiveSources.push_back(as);
    return src;
}

SoundSource SoundSystem::Play3D(const std::string& filepath,
                                SoundVec3 position,
                                float volume, float maxDist,
                                SoundCategory category)
{
    PlaySoundParams p;
    p.volume      = volume;
    p.is3D        = true;
    p.position    = position;
    p.maxDistance = maxDist;
    p.category    = category;
    return Play(filepath, p);
}

SoundSource SoundSystem::PlayWithPitchVariation(const std::string& filepath,
                                                float pitchMin, float pitchMax,
                                                float volume, SoundCategory category)
{
    PlaySoundParams p;
    p.volume   = volume;
    p.pitchMin = pitchMin;
    p.pitchMax = pitchMax;
    p.category = category;
    return Play(filepath, p);
}

SoundSource SoundSystem::PlayRandom(const std::vector<std::string>& filepaths,
                                    const PlaySoundParams& params)
{
    if (filepaths.empty()) return 0;
    size_t idx = static_cast<size_t>(RandomFloat(0.f, static_cast<float>(filepaths.size()) - 0.001f));
    return Play(filepaths[idx], params);
}

void SoundSystem::StopSound(SoundSource source) {
    alSourceStop(source);
    alDeleteSources(1, &source);
    m_ActiveSources.erase(
        std::remove_if(m_ActiveSources.begin(), m_ActiveSources.end(),
            [source](const ActiveSource& as) { return as.handle == source; }),
        m_ActiveSources.end());
}

void SoundSystem::FadeOut(SoundSource source, float seconds) {
    for (auto& as : m_ActiveSources) {
        if (as.handle == source) {
            as.fadeTarget    = 0.f;
            as.fadeDuration  = seconds;
            as.fadeElapsed   = 0.f;
            as.stopAfterFade = true;
            return;
        }
    }
}

void SoundSystem::FadeIn(SoundSource source, float seconds) {
    for (auto& as : m_ActiveSources) {
        if (as.handle == source) {
            as.fadeTarget   = 1.f;
            as.fadeDuration = seconds;
            as.fadeElapsed  = 0.f;
            return;
        }
    }
}

void SoundSystem::PauseSound(SoundSource source)  { alSourcePause(source); }
void SoundSystem::ResumeSound(SoundSource source) {
    ALint state;
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    if (state == AL_PAUSED) alSourcePlay(source);
}

void SoundSystem::SetSourceVolume(SoundSource source, float volume) {
    alSourcef(source, AL_GAIN, std::max(0.f, std::min(1.f, volume)) * m_VolumeMaster);
}

void SoundSystem::SetSourcePitch(SoundSource source, float pitch) {
    alSourcef(source, AL_PITCH, std::max(0.01f, pitch));
}

void SoundSystem::SetSourcePosition(SoundSource source, SoundVec3 pos) {
    alSource3f(source, AL_POSITION, pos.x, pos.y, pos.z);
}

void SoundSystem::SetSourceVelocity(SoundSource source, SoundVec3 vel) {
    alSource3f(source, AL_VELOCITY, vel.x, vel.y, vel.z);
}

bool SoundSystem::IsPlaying(SoundSource source) const {
    ALint state;
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

// ── Listener ──────────────────────────────────────────────────────────────────

void SoundSystem::SetListenerPosition(SoundVec3 pos) {
    alListener3f(AL_POSITION, pos.x, pos.y, pos.z);
}

void SoundSystem::SetListenerVelocity(SoundVec3 vel) {
    alListener3f(AL_VELOCITY, vel.x, vel.y, vel.z);
}

void SoundSystem::SetListenerOrientation(SoundVec3 forward, SoundVec3 up) {
    float orientation[6] = {
        forward.x, forward.y, forward.z,
        up.x,      up.y,      up.z
    };
    alListenerfv(AL_ORIENTATION, orientation);
}

// ── Globale Steuerung ─────────────────────────────────────────────────────────

void SoundSystem::PauseAll() {
    for (auto& as : m_ActiveSources) alSourcePause(as.handle);
    if (m_BiomeSource)     alSourcePause(m_BiomeSource);
    if (m_BiomeFadeSource) alSourcePause(m_BiomeFadeSource);
}

void SoundSystem::ResumeAll() {
    for (auto& as : m_ActiveSources) {
        ALint state;
        alGetSourcei(as.handle, AL_SOURCE_STATE, &state);
        if (state == AL_PAUSED) alSourcePlay(as.handle);
    }
    auto resumeBiome = [](SoundSource src) {
        if (!src) return;
        ALint state;
        alGetSourcei(src, AL_SOURCE_STATE, &state);
        if (state == AL_PAUSED) alSourcePlay(src);
    };
    resumeBiome(m_BiomeSource);
    resumeBiome(m_BiomeFadeSource);
}

void SoundSystem::StopAll() {
    for (auto& as : m_ActiveSources) {
        alSourceStop(as.handle);
        alDeleteSources(1, &as.handle);
    }
    m_ActiveSources.clear();
}

// ── Biome-System ──────────────────────────────────────────────────────────────

void SoundSystem::RegisterBiomeSound(BiomeType biome, const std::string& filepath) {
    m_BiomeSounds[biome] = filepath;
    LoadSound(filepath);
    spdlog::info("[SoundSystem] Registered biome sound: {}", filepath);
}

void SoundSystem::RegisterBiomePlaylist(BiomeType biome, BiomePlaylist playlist) {
    for (const auto& track : playlist.tracks)
        LoadSound(track);
    m_BiomePlaylists[biome] = std::move(playlist);
    spdlog::info("[SoundSystem] Registered biome playlist ({} tracks)", m_BiomePlaylists[biome].tracks.size());
}

void SoundSystem::SetActiveBiome(BiomeType biome) {
    if (biome == m_CurrentBiome) return;

    // Alten Loop ausblenden (Crossfade)
    if (m_BiomeSource != 0) {
        if (m_BiomeFadeSource != 0) {
            alSourceStop(m_BiomeFadeSource);
            alDeleteSources(1, &m_BiomeFadeSource);
        }
        m_BiomeFadeSource   = m_BiomeSource;
        m_BiomeFadeTimer    = 0.f;
        m_BiomeFadeDuration = kCROSSFADE_TIME;
        m_BiomeSource       = 0;
    }

    m_CurrentBiome = biome;
    if (biome == BiomeType::None) return;

    // Playlist bevorzugen, sonst Single-File
    std::string track;
    auto playlistIt = m_BiomePlaylists.find(biome);
    if (playlistIt != m_BiomePlaylists.end() && !playlistIt->second.tracks.empty()) {
        auto& pl = playlistIt->second;
        if (pl.shuffle)
            pl.currentIndex = static_cast<size_t>(RandomFloat(0.f, static_cast<float>(pl.tracks.size()) - 0.001f));
        track = pl.tracks[pl.currentIndex];
    } else {
        auto it = m_BiomeSounds.find(biome);
        if (it == m_BiomeSounds.end()) {
            spdlog::warn("[SoundSystem] No sound for biome {}", static_cast<int>(biome));
            return;
        }
        track = it->second;
    }

    ALuint buffer = LoadSound(track);
    if (buffer == 0) return;

    alGenSources(1, &m_BiomeSource);
    alSourcei (m_BiomeSource, AL_BUFFER,          static_cast<ALint>(buffer));
    alSourcef (m_BiomeSource, AL_GAIN,            0.f);   // Startet lautlos (Crossfade)
    alSourcef (m_BiomeSource, AL_PITCH,           1.0f);
    alSourcei (m_BiomeSource, AL_LOOPING,         AL_TRUE);
    alSourcei (m_BiomeSource, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(m_BiomeSource, AL_POSITION,        0.f, 0.f, 0.f);
    alSourcef (m_BiomeSource, AL_ROLLOFF_FACTOR,  0.f);
    alSourcePlay(m_BiomeSource);

    spdlog::info("[SoundSystem] Biome crossfade → {}", track);
}

void SoundSystem::StopBiomeSound() {
    auto del = [](SoundSource& src) {
        if (src != 0) {
            alSourceStop(src);
            alDeleteSources(1, &src);
            src = 0;
        }
    };
    del(m_BiomeSource);
    del(m_BiomeFadeSource);
    m_CurrentBiome      = BiomeType::None;
    m_BiomeFadeTimer    = 0.f;
    m_BiomeFadeDuration = 0.f;
}

void SoundSystem::SetBiomeVolume(float volume) {
    m_BiomeBaseVolume = std::max(0.f, std::min(1.f, volume));
    if (m_BiomeSource)
        alSourcef(m_BiomeSource, AL_GAIN,
                  EffectiveVolume(m_BiomeBaseVolume, SoundCategory::Ambient));
}

// ── Lautstärke-Kategorien ─────────────────────────────────────────────────────

void SoundSystem::SetCategoryVolume(SoundCategory category, float volume) {
    float v = std::max(0.f, std::min(1.f, volume));
    switch (category) {
        case SoundCategory::Master:  SetMasterVolume(v); return;
        case SoundCategory::Effects: m_VolumeEffects = v; break;
        case SoundCategory::Ambient: m_VolumeAmbient = v; break;
        case SoundCategory::UI:      m_VolumeUI      = v; break;
        case SoundCategory::Music:   m_VolumeMusic   = v; break;
    }
    // Biome-Loop sofort aktualisieren wenn Ambient geändert
    if (category == SoundCategory::Ambient && m_BiomeSource)
        alSourcef(m_BiomeSource, AL_GAIN,
                  EffectiveVolume(m_BiomeBaseVolume, SoundCategory::Ambient));
}

float SoundSystem::GetCategoryVolume(SoundCategory category) const {
    switch (category) {
        case SoundCategory::Master:  return m_VolumeMaster;
        case SoundCategory::Effects: return m_VolumeEffects;
        case SoundCategory::Ambient: return m_VolumeAmbient;
        case SoundCategory::UI:      return m_VolumeUI;
        case SoundCategory::Music:   return m_VolumeMusic;
    }
    return 1.0f;
}

void SoundSystem::SetMasterVolume(float volume) {
    m_VolumeMaster = std::max(0.f, std::min(1.f, volume));
    alListenerf(AL_GAIN, m_VolumeMaster);
}

// ── Ducking ───────────────────────────────────────────────────────────────────

void SoundSystem::DuckCategory(SoundCategory category, float factor, float duration) {
    m_DuckStates[static_cast<int>(category)] = { factor, duration };
    spdlog::debug("[SoundSystem] Ducking category {} → {:.2f} for {:.1f}s",
                  static_cast<int>(category), factor, duration);
}

// ── Update ────────────────────────────────────────────────────────────────────

void SoundSystem::Update(float deltaTime) {
    if (!m_Initialized) return;

    m_GlobalTime += deltaTime;

    UpdateFades(deltaTime);
    UpdateBiomeCrossfade(deltaTime);
    CleanupStoppedSources();

    // Ducking-Timer heruntertzählen
    for (auto& [cat, duck] : m_DuckStates) {
        if (duck.remaining > 0.f) {
            duck.remaining -= deltaTime;
            if (duck.remaining <= 0.f)
                duck.factor = 1.f;
        }
    }
}

void SoundSystem::UpdateFades(float dt) {
    for (auto& as : m_ActiveSources) {
        if (as.fadeDuration <= 0.f) continue;

        as.fadeElapsed += dt;
        float t = std::min(as.fadeElapsed / as.fadeDuration, 1.f);

        float currentGain;
        alGetSourcef(as.handle, AL_GAIN, &currentGain);
        float targetGain = EffectiveVolume(as.baseVolume * as.fadeTarget, as.category);
        float newGain    = currentGain + (targetGain - currentGain) * t;

        alSourcef(as.handle, AL_GAIN, newGain);

        if (as.fadeElapsed >= as.fadeDuration) {
            as.fadeDuration = 0.f;
            if (as.stopAfterFade) {
                alSourceStop(as.handle);
            }
        }
    }
}

void SoundSystem::UpdateBiomeCrossfade(float dt) {
    if (m_BiomeFadeDuration <= 0.f) return;

    m_BiomeFadeTimer += dt;
    float t = std::min(m_BiomeFadeTimer / m_BiomeFadeDuration, 1.f);

    // Altes Biome → ausblenden
    if (m_BiomeFadeSource) {
        float gain = EffectiveVolume(m_BiomeBaseVolume * (1.f - t), SoundCategory::Ambient);
        alSourcef(m_BiomeFadeSource, AL_GAIN, std::max(0.f, gain));
    }

    // Neues Biome → einblenden
    if (m_BiomeSource) {
        float gain = EffectiveVolume(m_BiomeBaseVolume * t, SoundCategory::Ambient);
        alSourcef(m_BiomeSource, AL_GAIN, gain);
    }

    if (t >= 1.f) {
        m_BiomeFadeDuration = 0.f;
        if (m_BiomeFadeSource) {
            alSourceStop(m_BiomeFadeSource);
            alDeleteSources(1, &m_BiomeFadeSource);
            m_BiomeFadeSource = 0;
        }
    }
}

void SoundSystem::CleanupStoppedSources() {
    m_ActiveSources.erase(
        std::remove_if(m_ActiveSources.begin(), m_ActiveSources.end(),
            [](ActiveSource& as) {
                ALint state;
                alGetSourcei(as.handle, AL_SOURCE_STATE, &state);
                if (state == AL_STOPPED && !as.stopAfterFade) {
                    alDeleteSources(1, &as.handle);
                    return true;
                }
                // Bereits durch FadeOut gestoppt
                if (state == AL_STOPPED && as.stopAfterFade && as.fadeDuration <= 0.f) {
                    alDeleteSources(1, &as.handle);
                    return true;
                }
                return false;
            }),
        m_ActiveSources.end());
}
