/**
 * @file SoundEvents.h
 * @brief Game/UI events that the SoundSystem can react to automatically.
 */

#pragma once
#include "../Core/Events/Event.h"
#include "SoundSystem.h"

// ═══════════════════════════════════════════════════════════════════════════════
// SoundEvents.h
//
// Events die das SoundSystem automatisch triggern kann.
// ═══════════════════════════════════════════════════════════════════════════════


// ── EntityDamagedEvent ────────────────────────────────────────────────────────

/**
 * @class EntityDamagedEvent
 * @brief Fired when an entity takes damage; drives hit sounds.
 */
class EntityDamagedEvent : public Event {
public:
    /**
     * @brief Constructs the event.
     * @param amount     Damage dealt.
     * @param isCritical Whether it was a critical hit.
     * @param position   World position of the hit (for 3D audio).
     */
    explicit EntityDamagedEvent(float amount,
                                bool  isCritical = false,
                                SoundVec3 position = {})
        : m_Amount(amount), m_IsCritical(isCritical), m_Position(position) {}

    float     GetAmount()   const { return m_Amount;    } ///< @return Damage amount.
    bool      IsCritical()  const { return m_IsCritical; } ///< @return Whether it was critical.
    SoundVec3 GetPosition() const { return m_Position;  } ///< @return Hit world position.

    EVENT_CLASS_TYPE(EntityDamaged)
    EVENT_CLASS_CATEGORY(EventCategory::Game | EventCategory::Sound)

private:
    float     m_Amount;     ///< Damage dealt.
    bool      m_IsCritical; ///< Whether the hit was critical.
    SoundVec3 m_Position;   ///< World position of the hit.
};

// ── EntityDiedEvent ───────────────────────────────────────────────────────────

/**
 * @class EntityDiedEvent
 * @brief Fired when an entity dies; drives death sounds.
 */
class EntityDiedEvent : public Event {
public:
    /// @param position World position of the death.
    explicit EntityDiedEvent(SoundVec3 position = {})
        : m_Position(position) {}

    SoundVec3 GetPosition() const { return m_Position; } ///< @return Death world position.

    EVENT_CLASS_TYPE(EntityDied)
    EVENT_CLASS_CATEGORY(EventCategory::Game | EventCategory::Sound)

private:
    SoundVec3 m_Position; ///< World position of the death.
};

// ── BiomeChangedEvent ─────────────────────────────────────────────────────────

/**
 * @class BiomeChangedEvent
 * @brief Fired when the listener enters a new biome; drives ambient crossfade.
 */
class BiomeChangedEvent : public Event {
public:
    /**
     * @brief Constructs the event.
     * @param newBiome Biome just entered.
     * @param oldBiome Biome left (None if unknown).
     */
    explicit BiomeChangedEvent(BiomeType newBiome, BiomeType oldBiome = BiomeType::None)
        : m_NewBiome(newBiome), m_OldBiome(oldBiome) {}

    BiomeType GetBiome()    const { return m_NewBiome; } ///< @return Newly entered biome.
    BiomeType GetOldBiome() const { return m_OldBiome; } ///< @return Previously active biome.

    EVENT_CLASS_TYPE(BiomeChanged)
    EVENT_CLASS_CATEGORY(EventCategory::Game | EventCategory::Sound)

private:
    BiomeType m_NewBiome; ///< Newly entered biome.
    BiomeType m_OldBiome; ///< Previously active biome.
};

// ── FootstepEvent ─────────────────────────────────────────────────────────────

/**
 * @enum SurfaceType
 * @brief Ground material under a footstep; selects the footstep sound set.
 */
enum class SurfaceType { Stone, Grass, Wood, Sand, Water, Metal };

/**
 * @class FootstepEvent
 * @brief Fired on each footstep; drives surface-dependent footstep sounds.
 */
class FootstepEvent : public Event {
public:
    /**
     * @brief Constructs the event.
     * @param surface  Surface material stepped on.
     * @param position World position of the step.
     * @param speed    Movement speed (may scale volume/pitch).
     */
    explicit FootstepEvent(SurfaceType surface, SoundVec3 position = {}, float speed = 1.f)
        : m_Surface(surface), m_Position(position), m_Speed(speed) {}

    SurfaceType GetSurface()  const { return m_Surface;  } ///< @return Surface material.
    SoundVec3   GetPosition() const { return m_Position; } ///< @return Step world position.
    float       GetSpeed()    const { return m_Speed;    } ///< @return Movement speed.

    EVENT_CLASS_TYPE(Footstep)
    EVENT_CLASS_CATEGORY(EventCategory::Game | EventCategory::Sound)

private:
    SurfaceType m_Surface;  ///< Surface material stepped on.
    SoundVec3   m_Position; ///< World position of the step.
    float       m_Speed;    ///< Movement speed.
};

// ── UIClickEvent ──────────────────────────────────────────────────────────────

/**
 * @class UIClickEvent
 * @brief Fired on a UI click; drives the click sound.
 */
class UIClickEvent : public Event {
public:
    UIClickEvent() = default; ///< Default constructor.
    EVENT_CLASS_TYPE(UIClick)
    EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Sound)
};

// ── MusicRequestEvent ─────────────────────────────────────────────────────────

/**
 * @class MusicRequestEvent
 * @brief Requests a music track change with optional crossfade.
 */
class MusicRequestEvent : public Event {
public:
    /**
     * @brief Constructs the event.
     * @param filepath Music file to play.
     * @param fadeIn   Fade-in seconds.
     * @param fadeOut  Fade-out seconds for the previous track.
     * @param loop     Whether the track loops.
     */
    explicit MusicRequestEvent(const std::string& filepath,
                               float fadeIn  = 1.5f,
                               float fadeOut = 1.5f,
                               bool  loop    = true)
        : m_Filepath(filepath), m_FadeIn(fadeIn), m_FadeOut(fadeOut), m_Loop(loop) {}

    const std::string& GetFilepath() const { return m_Filepath; } ///< @return Music file path.
    float GetFadeIn()  const { return m_FadeIn;  } ///< @return Fade-in seconds.
    float GetFadeOut() const { return m_FadeOut; } ///< @return Fade-out seconds.
    bool  GetLoop()    const { return m_Loop;    } ///< @return Whether the track loops.

    EVENT_CLASS_TYPE(MusicRequest)
    EVENT_CLASS_CATEGORY(EventCategory::Sound)

private:
    std::string m_Filepath; ///< Music file to play.
    float m_FadeIn;         ///< Fade-in seconds.
    float m_FadeOut;        ///< Fade-out seconds.
    bool  m_Loop;           ///< Whether the track loops.
};
