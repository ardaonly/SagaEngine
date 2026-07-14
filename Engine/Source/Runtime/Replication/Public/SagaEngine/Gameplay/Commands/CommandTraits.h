/// @file CommandTraits.h
/// @brief Compile-time metadata (reliability, auth, rate-limit) for each OpCode.
///
/// Layer  : SagaEngine / Gameplay / Commands
/// Purpose: The GameplayCommandDispatcher consults these traits at register
///          time to wire every opcode into the correct reliable/unreliable
///          channel, to gate it on authentication, and to seed rate limits.
///          Putting this at compile time prevents entire classes of bugs —
///          you cannot ship a command without deciding its reliability.
///
/// Design rules:
///   - Every OpCode MUST have a specialisation. The primary template is
///     intentionally incomplete so missing specialisations fail to compile.
///   - Changing a trait is a wire-compat decision. Bump the command's
///     schemaVersion when reliability or auth semantics change.
///   - rateLimitPerSecond is a *per-client* ceiling. 0 means "disabled".

#pragma once

#include "SagaEngine/Gameplay/Commands/OpCode.h"

#include <cstdint>

namespace SagaEngine::Gameplay::Commands
{

// ─── Reliability channel ──────────────────────────────────────────────

/// How the transport should treat a gameplay command.
enum class Reliability : std::uint8_t
{
    Unreliable        = 0,  ///< Fire and forget (drop on loss).
    UnreliableOrdered = 1,  ///< Dropped if late, ordered otherwise.
    Reliable          = 2,  ///< Retransmit until acked; ordered.
};

// ─── Primary (undefined on purpose) ───────────────────────────────────

/// Primary template is intentionally missing a body.
/// Any OpCode without a specialisation produces a compile error, which is
/// the whole point: wire traits are not allowed to be implicit.
template <OpCode Op>
struct CommandTraits;

// ─── Specialisations ──────────────────────────────────────────────────

template <>
struct CommandTraits<OpCode::MoveRequest>
{
    static constexpr Reliability reliability         = Reliability::Reliable;
    static constexpr bool        requiresAuth        = true;
    static constexpr std::uint32_t rateLimitPerSecond = 20;   ///< Bursty path updates tolerated.
    static constexpr const char* debugName           = "MoveRequest";
};

template <>
struct CommandTraits<OpCode::CastSpell>
{
    static constexpr Reliability reliability         = Reliability::Reliable;
    static constexpr bool        requiresAuth        = true;
    static constexpr std::uint32_t rateLimitPerSecond = 10;
    static constexpr const char* debugName           = "CastSpell";
};

template <>
struct CommandTraits<OpCode::Interact>
{
    static constexpr Reliability reliability         = Reliability::Reliable;
    static constexpr bool        requiresAuth        = true;
    static constexpr std::uint32_t rateLimitPerSecond = 15;
    static constexpr const char* debugName           = "Interact";
};

template <>
struct CommandTraits<OpCode::Trade>
{
    static constexpr Reliability reliability         = Reliability::Reliable;
    static constexpr bool        requiresAuth        = true;
    static constexpr std::uint32_t rateLimitPerSecond = 5;    ///< Trade state machine is slow.
    static constexpr const char* debugName           = "Trade";
};

template <>
struct CommandTraits<OpCode::UseItem>
{
    static constexpr Reliability reliability         = Reliability::Reliable;
    static constexpr bool        requiresAuth        = true;
    static constexpr std::uint32_t rateLimitPerSecond = 10;
    static constexpr const char* debugName           = "UseItem";
};

template <>
struct CommandTraits<OpCode::Equip>
{
    static constexpr Reliability reliability         = Reliability::Reliable;
    static constexpr bool        requiresAuth        = true;
    static constexpr std::uint32_t rateLimitPerSecond = 8;
    static constexpr const char* debugName           = "Equip";
};

template <>
struct CommandTraits<OpCode::ChatMessage>
{
    static constexpr Reliability reliability         = Reliability::Reliable;
    static constexpr bool        requiresAuth        = true;
    static constexpr std::uint32_t rateLimitPerSecond = 4;    ///< Anti-spam upstream of moderation.
    static constexpr const char* debugName           = "ChatMessage";
};

// ─── Runtime-queryable view ───────────────────────────────────────────

/// Runtime mirror of CommandTraits, populated once at dispatcher register
/// time. Kept separate from the template so dispatcher code can iterate
/// uniformly over opcodes without template gymnastics in the hot path.
struct CommandTraitsView
{
    OpCode         opCode             = OpCode::None;
    Reliability    reliability        = Reliability::Reliable;
    bool           requiresAuth       = true;
    std::uint32_t  rateLimitPerSecond = 0;
    const char*    debugName          = "Unknown";
};

/// Produce a runtime view from a compile-time specialisation.
template <OpCode Op>
[[nodiscard]] constexpr CommandTraitsView MakeTraitsView() noexcept
{
    return CommandTraitsView{
        Op,
        CommandTraits<Op>::reliability,
        CommandTraits<Op>::requiresAuth,
        CommandTraits<Op>::rateLimitPerSecond,
        CommandTraits<Op>::debugName,
    };
}

} // namespace SagaEngine::Gameplay::Commands
