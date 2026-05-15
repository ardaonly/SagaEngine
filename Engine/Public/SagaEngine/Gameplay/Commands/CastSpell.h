/// @file CastSpell.h
/// @brief Client intent to cast a spell or ability on a target or location.
///
/// Layer  : SagaEngine / Gameplay / Commands
/// Purpose: Pure intent — the server validates everything: GCD, cooldowns,
///          mana/resource cost, line of sight, range, silence/stun, target
///          validity, etc. None of that lives here.
///
/// Schema v1:
///   header(4) | spellId(4) | targetEntity(4) | targetX(4) | targetY(4)
///           | targetZ(4) | clientCastStartMs(8) | comboFlags(1)

#pragma once

#include "SagaEngine/Gameplay/Commands/CommandCodec.h"

#include <cstdint>

namespace SagaEngine::Gameplay::Commands
{

// ─── CastSpell ────────────────────────────────────────────────────────

/// Schema v1 payload for OpCode::CastSpell.
struct CastSpell
{
    static constexpr OpCode        kOpCode        = OpCode::CastSpell;
    static constexpr std::uint16_t kSchemaVersion = 1;

    std::uint32_t spellId            = 0;
    std::uint32_t targetEntity       = 0;      ///< 0 for ground-targeted / self-cast.
    float         targetX            = 0.0f;
    float         targetY            = 0.0f;
    float         targetZ            = 0.0f;
    std::uint64_t clientCastStartMs  = 0;      ///< Client-side ms since epoch; server reconciles.
    std::uint8_t  comboFlags         = 0;      ///< Stance / combo context bits; spell-specific.

    void Encode(ByteWriter& w) const
    {
        w.WriteHeader(kOpCode, kSchemaVersion);
        w.WriteU32(spellId);
        w.WriteU32(targetEntity);
        w.WriteFloat(targetX);
        w.WriteFloat(targetY);
        w.WriteFloat(targetZ);
        w.WriteU64(clientCastStartMs);
        w.WriteU8(comboFlags);
    }

    [[nodiscard]] static bool Decode(ByteReader& r, CastSpell& out)
    {
        std::uint16_t version = 0;
        if (!r.ReadHeader(kOpCode, version)) return false;
        if (version != kSchemaVersion)       return false;

        out.spellId           = r.ReadU32();
        out.targetEntity      = r.ReadU32();
        out.targetX           = r.ReadFloat();
        out.targetY           = r.ReadFloat();
        out.targetZ           = r.ReadFloat();
        out.clientCastStartMs = r.ReadU64();
        out.comboFlags        = r.ReadU8();
        return r.Ok();
    }
};

} // namespace SagaEngine::Gameplay::Commands
