/// @file AudioEventMapper.h
/// @brief Maps gameplay events (server intents) into AudioEvents.
///
/// This is the "Client Audio Event Layer" — the most important layer
/// in the pipeline.  It takes raw gameplay intents like WeaponFired or
/// Footstep and produces fully contextualised AudioEvents with the
/// correct Wwise event name, RTPCs, switches, and priority.
///
/// Design:
///   - The server NEVER sends audio commands — only gameplay state.
///   - This mapper adds context: distance, 1P/3P, surface, environment.
///   - One gameplay event can produce 0..N audio events (e.g. explosion
///     may produce both a boom and shrapnel SFX).

#pragma once

#include "SagaEngine/Audio/AudioEvent.h"
#include "SagaEngine/Audio/AudioTypes.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Audio
{

// ── Gameplay intent descriptors (server-side neutral) ────────────────

/// Generic gameplay event that arrives from the event bus.
/// The mapper inspects `type` to decide which audio events to produce.
struct GameplayAudioIntent
{
    std::string    type;           ///< e.g. "WeaponFired", "Footstep", "EntityDied"
    AudioObjectId  sourceObject = AudioObjectId::kInvalid;
    AudioTransform sourceTransform{};

    // ── Optional payload fields (filled depending on type) ───────
    std::uint32_t  weaponId     = 0;
    std::uint32_t  spellId      = 0;
    SurfaceType    surfaceType  = SurfaceType::Default;
    float          intensity    = 1.f;   ///< 0..1 scale hint.
};

/// Context about the local player / listener — the mapper uses this
/// to make 1P vs 3P decisions, distance culling, etc.
struct ListenerContext
{
    AudioTransform  listenerTransform{};
    EnvironmentType environment = EnvironmentType::Outdoor;
    bool            isFirstPerson = true;
    bool            isInMenu      = false;
    float           masterVolume  = 1.f;
};

// ── Mapping rule ─────────────────────────────────────────────────────

/// A user-registered mapping function.  Takes the intent + listener
/// context and appends zero or more AudioEvents to `out`.
using MappingFn = std::function<void(const GameplayAudioIntent& intent,
                                     const ListenerContext& ctx,
                                     AudioEventBatch& out)>;

// ── AudioEventMapper ─────────────────────────────────────────────────

class AudioEventMapper
{
public:
    /// Register a mapping rule for a gameplay event type.
    void Register(const std::string& intentType, MappingFn fn);

    /// Map a gameplay intent into audio events using the registered rules.
    void Map(const GameplayAudioIntent& intent,
             const ListenerContext& ctx,
             AudioEventBatch& out) const;

    /// Convenience — map a whole batch of intents.
    void MapAll(const std::vector<GameplayAudioIntent>& intents,
                const ListenerContext& ctx,
                AudioEventBatch& out) const;

    /// Register the built-in default mappings (WeaponFired, Footstep,
    /// SpellCast, EntityDied, Impact).
    void RegisterDefaults();

    [[nodiscard]] std::size_t RuleCount() const noexcept { return m_rules.size(); }

private:
    std::unordered_map<std::string, std::vector<MappingFn>> m_rules;
};

} // namespace SagaEngine::Audio
