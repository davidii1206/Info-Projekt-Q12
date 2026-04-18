#pragma once
#include "../Core/Events/Event.h"
#include "SoundSystem.h"



// ── EntityDamagedEvent ────────────────────────────────────────────────────────

class EntityDamagedEvent : public Event {
public:
    explicit EntityDamagedEvent(float amount, bool isCritical = false)
        : m_Amount(amount), m_IsCritical(isCritical) {}

    float GetAmount()    const { return m_Amount;     }
    bool  IsCritical()   const { return m_IsCritical; }

    EVENT_CLASS_TYPE(None) // Ersetze "None" mit deinem eigenen EventType-Wert
    EVENT_CLASS_CATEGORY(EventCategory::None)

private:
    float m_Amount;
    bool  m_IsCritical;
};

// ── EntityDiedEvent ───────────────────────────────────────────────────────────

class EntityDiedEvent : public Event {
public:
    EntityDiedEvent() = default;

    EVENT_CLASS_TYPE(None)
    EVENT_CLASS_CATEGORY(EventCategory::None)
};

// ── BiomeChangedEvent ─────────────────────────────────────────────────────────

class BiomeChangedEvent : public Event {
public:
    explicit BiomeChangedEvent(BiomeType newBiome) : m_NewBiome(newBiome) {}

    BiomeType GetBiome() const { return m_NewBiome; }

    EVENT_CLASS_TYPE(None)
    EVENT_CLASS_CATEGORY(EventCategory::None)

private:
    BiomeType m_NewBiome;
};
