#pragma once
#include "../Core/Events/Event.h"
#include "SoundSystem.h"

// ═══════════════════════════════════════════════════════════════════════════════
// SoundEvents.h
//
// Events die das SoundSystem automatisch triggern kann.
// Registrierung im GameLayer (oder ähnlichem):
//
//   EventDispatcher d(event);
//   d.Dispatch<EntityDamagedEvent>([](EntityDamagedEvent& e) {
//       PlaySoundParams p;
//       p.pitchMin = 0.9f; p.pitchMax = 1.15f;
//       p.priority = e.IsCritical() ? SoundPriority::High : SoundPriority::Normal;
//       if (e.IsCritical())
//           SoundSystem::Get().Play("assets/audio/sfx_hit_critical.wav", p);
//       else
//           SoundSystem::Get().Play("assets/audio/sfx_hit.wav", p);
//       return false;
//   });
//
// ═══════════════════════════════════════════════════════════════════════════════


// ── EntityDamagedEvent ────────────────────────────────────────────────────────

class EntityDamagedEvent : public Event {
public:
    explicit EntityDamagedEvent(float amount,
                                bool  isCritical = false,
                                SoundVec3 position = {})
        : m_Amount(amount), m_IsCritical(isCritical), m_Position(position) {}

    float     GetAmount()   const { return m_Amount;    }
    bool      IsCritical()  const { return m_IsCritical; }
    SoundVec3 GetPosition() const { return m_Position;  }

    EVENT_CLASS_TYPE(None)
    EVENT_CLASS_CATEGORY(EventCategory::None)

private:
    float     m_Amount;
    bool      m_IsCritical;
    SoundVec3 m_Position;
};

// ── EntityDiedEvent ───────────────────────────────────────────────────────────

class EntityDiedEvent : public Event {
public:
    explicit EntityDiedEvent(SoundVec3 position = {})
        : m_Position(position) {}

    SoundVec3 GetPosition() const { return m_Position; }

    EVENT_CLASS_TYPE(None)
    EVENT_CLASS_CATEGORY(EventCategory::None)

private:
    SoundVec3 m_Position;
};

// ── BiomeChangedEvent ─────────────────────────────────────────────────────────

class BiomeChangedEvent : public Event {
public:
    explicit BiomeChangedEvent(BiomeType newBiome, BiomeType oldBiome = BiomeType::None)
        : m_NewBiome(newBiome), m_OldBiome(oldBiome) {}

    BiomeType GetBiome()    const { return m_NewBiome; }
    BiomeType GetOldBiome() const { return m_OldBiome; }

    EVENT_CLASS_TYPE(None)
    EVENT_CLASS_CATEGORY(EventCategory::None)

private:
    BiomeType m_NewBiome;
    BiomeType m_OldBiome;
};

// ── FootstepEvent ─────────────────────────────────────────────────────────────
// Wird z.B. vom Animation-System pro Schritt gefeuert.

enum class SurfaceType { Stone, Grass, Wood, Sand, Water, Metal };

class FootstepEvent : public Event {
public:
    explicit FootstepEvent(SurfaceType surface, SoundVec3 position = {}, float speed = 1.f)
        : m_Surface(surface), m_Position(position), m_Speed(speed) {}

    SurfaceType GetSurface()  const { return m_Surface;  }
    SoundVec3   GetPosition() const { return m_Position; }
    float       GetSpeed()    const { return m_Speed;    }

    EVENT_CLASS_TYPE(None)
    EVENT_CLASS_CATEGORY(EventCategory::None)

private:
    SurfaceType m_Surface;
    SoundVec3   m_Position;
    float       m_Speed;
};

// ── UIClickEvent ──────────────────────────────────────────────────────────────

class UIClickEvent : public Event {
public:
    UIClickEvent() = default;
    EVENT_CLASS_TYPE(None)
    EVENT_CLASS_CATEGORY(EventCategory::None)
};

// ── MusicRequestEvent ─────────────────────────────────────────────────────────
// Starte ein Musikstück mit optionalem Fade-In.

class MusicRequestEvent : public Event {
public:
    explicit MusicRequestEvent(const std::string& filepath,
                               float fadeIn  = 1.5f,
                               float fadeOut = 1.5f,
                               bool  loop    = true)
        : m_Filepath(filepath), m_FadeIn(fadeIn), m_FadeOut(fadeOut), m_Loop(loop) {}

    const std::string& GetFilepath() const { return m_Filepath; }
    float GetFadeIn()  const { return m_FadeIn;  }
    float GetFadeOut() const { return m_FadeOut; }
    bool  GetLoop()    const { return m_Loop;    }

    EVENT_CLASS_TYPE(None)
    EVENT_CLASS_CATEGORY(EventCategory::None)

private:
    std::string m_Filepath;
    float m_FadeIn;
    float m_FadeOut;
    bool  m_Loop;
};
