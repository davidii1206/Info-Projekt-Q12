#include "SoundSystem.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <vector>
#include <cstring>
#include <algorithm>

// ── Minimaler WAV-Parser ──────────────────────────────────────────────────────

struct WavHeader {
    char     riff[4];
    uint32_t chunkSize;
    char     wave[4];
    char     fmt[4];
    uint32_t fmtSize;
    uint16_t audioFormat;   // 1 = PCM
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

    // Springe zum "data"-Chunk (überspringt optionale Chunks wie "LIST", "fact")
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
    alListener3f(AL_POSITION, 0.f, 0.f, 0.f);
    alListener3f(AL_VELOCITY, 0.f, 0.f, 0.f);
    alListenerf(AL_GAIN, m_VolumeMaster);

    m_Initialized = true;
    spdlog::info("[SoundSystem] Initialized (OpenAL)");
    return true;
}

void SoundSystem::Shutdown() {
    if (!m_Initialized) return;

    StopBiomeSound();

    for (SoundSource src : m_ActiveSources) {
        alSourceStop(src);
        alDeleteSources(1, &src);
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
        default: break;
    }
    return base * catVol * m_VolumeMaster;
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

SoundSource SoundSystem::AcquireSource() {
    if (m_ActiveSources.size() >= kMAX_SOURCES) {
        spdlog::warn("[SoundSystem] Source pool exhausted ({} active)", kMAX_SOURCES);
        return 0;
    }
    ALuint src;
    alGenSources(1, &src);
    return src;
}

// ── Playback ──────────────────────────────────────────────────────────────────

SoundSource SoundSystem::PlaySound(const std::string& filepath,
                                   float volume, float pitch, bool loop,
                                   SoundCategory category)
{
    PlaySoundParams p;
    p.volume   = volume;
    p.pitch    = pitch;
    p.pitchMin = pitch;
    p.pitchMax = pitch;
    p.loop     = loop;
    p.category = category;
    return Play(filepath, p);
}

SoundSource SoundSystem::Play(const std::string& filepath, const PlaySoundParams& params) {
    if (!m_Initialized) return 0;

    ALuint buffer = LoadSound(filepath);
    if (buffer == 0) return 0;

    SoundSource src = AcquireSource();
    if (src == 0) return 0;

    // Pitch: wenn Min != Max → zufälliger Wert im Bereich, sonst fixer Wert
    float pitch = (params.pitchMin != params.pitchMax)
                ? RandomFloat(params.pitchMin, params.pitchMax)
                : params.pitch;

    alSourcei (src, AL_BUFFER,   static_cast<ALint>(buffer));
    alSourcef (src, AL_GAIN,     EffectiveVolume(params.volume, params.category));
    alSourcef (src, AL_PITCH,    pitch);
    alSourcei (src, AL_LOOPING,  params.loop ? AL_TRUE : AL_FALSE);
    alSource3f(src, AL_POSITION, 0.f, 0.f, 0.f);
    alSourcePlay(src);

    m_ActiveSources.push_back(src);
    return src;
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

void SoundSystem::StopSound(SoundSource source) {
    alSourceStop(source);
    alDeleteSources(1, &source);
    auto it = std::find(m_ActiveSources.begin(), m_ActiveSources.end(), source);
    if (it != m_ActiveSources.end()) m_ActiveSources.erase(it);
}

void SoundSystem::PauseSound(SoundSource source) {
    alSourcePause(source);
}

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

bool SoundSystem::IsPlaying(SoundSource source) const {
    ALint state;
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

// ── Globale Steuerung ─────────────────────────────────────────────────────────

void SoundSystem::PauseAll() {
    for (SoundSource src : m_ActiveSources) alSourcePause(src);
    if (m_BiomeSource) alSourcePause(m_BiomeSource);
}

void SoundSystem::ResumeAll() {
    for (SoundSource src : m_ActiveSources) {
        ALint state;
        alGetSourcei(src, AL_SOURCE_STATE, &state);
        if (state == AL_PAUSED) alSourcePlay(src);
    }
    if (m_BiomeSource) {
        ALint state;
        alGetSourcei(m_BiomeSource, AL_SOURCE_STATE, &state);
        if (state == AL_PAUSED) alSourcePlay(m_BiomeSource);
    }
}

void SoundSystem::StopAll() {
    for (SoundSource src : m_ActiveSources) {
        alSourceStop(src);
        alDeleteSources(1, &src);
    }
    m_ActiveSources.clear();
}

// ── Biome-System ──────────────────────────────────────────────────────────────

void SoundSystem::RegisterBiomeSound(BiomeType biome, const std::string& filepath) {
    m_BiomeSounds[biome] = filepath;
    LoadSound(filepath); // Vorladen
    spdlog::info("[SoundSystem] Registered biome sound: {}", filepath);
}

void SoundSystem::SetActiveBiome(BiomeType biome) {
    if (biome == m_CurrentBiome) return;

    StopBiomeSound();
    m_CurrentBiome = biome;
    if (biome == BiomeType::None) return;

    auto it = m_BiomeSounds.find(biome);
    if (it == m_BiomeSounds.end()) {
        spdlog::warn("[SoundSystem] No sound for biome {}", static_cast<int>(biome));
        return;
    }

    ALuint buffer = LoadSound(it->second);
    if (buffer == 0) return;

    alGenSources(1, &m_BiomeSource);
    alSourcei (m_BiomeSource, AL_BUFFER,   static_cast<ALint>(buffer));
    alSourcef (m_BiomeSource, AL_GAIN,     EffectiveVolume(m_BiomeBaseVolume, SoundCategory::Ambient));
    alSourcef (m_BiomeSource, AL_PITCH,    1.0f);
    alSourcei (m_BiomeSource, AL_LOOPING,  AL_TRUE);
    alSourcePlay(m_BiomeSource);

    spdlog::info("[SoundSystem] Biome ambient: {}", it->second);
}

void SoundSystem::StopBiomeSound() {
    if (m_BiomeSource != 0) {
        alSourceStop(m_BiomeSource);
        alDeleteSources(1, &m_BiomeSource);
        m_BiomeSource = 0;
    }
    m_CurrentBiome = BiomeType::None;
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
    }
    spdlog::debug("[SoundSystem] Category {} volume → {:.2f}",
                  static_cast<int>(category), v);

    // Biome-Loop Lautstärke sofort aktualisieren wenn Ambient geändert wurde
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
    }
    return 1.0f;
}

void SoundSystem::SetMasterVolume(float volume) {
    m_VolumeMaster = std::max(0.f, std::min(1.f, volume));
    // OpenAL Listener-Gain = globaler Master
    alListenerf(AL_GAIN, m_VolumeMaster);
}

// ── Update ────────────────────────────────────────────────────────────────────

void SoundSystem::Update() {
    if (!m_Initialized) return;

    m_ActiveSources.erase(
        std::remove_if(m_ActiveSources.begin(), m_ActiveSources.end(),
            [](SoundSource src) {
                ALint state;
                alGetSourcei(src, AL_SOURCE_STATE, &state);
                if (state == AL_STOPPED) {
                    alDeleteSources(1, &src);
                    return true;
                }
                return false;
            }),
        m_ActiveSources.end());
}
