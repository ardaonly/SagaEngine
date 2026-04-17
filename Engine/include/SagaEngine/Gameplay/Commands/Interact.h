/// @file Interact.h
/// @brief Client intent to interact with a world entity (NPC, loot, door, object).
///
/// Layer  : SagaEngine / Gameplay / Commands
/// Purpose: Single unified command for "use this thing" semantics. The server
///          dispatches to the appropriate interaction handler based on the
///          target entity's components (dialogue, vendor, container, etc.).
///
/// Schema v1:
///   header(4) | kind(1) | targetEntity(4) | contextId(4) | optionIndex(2)

#pragma once

#include "SagaEngine/Gameplay/Commands/CommandCodec.h"

#include <cstdint>

namespace SagaEngine::Gameplay::Commands
{

// ─── Interact ─────────────────────────────────────────────────────────

/// High-level interaction verb. Server resolves full semantics per entity.
enum class InteractKind : std::uint8_t
{
    Default       = 0,  ///< Primary action (talk / open / loot / use).
    DialogueReply = 1,  ///< Selecting a dialogue option by optionIndex.
    OpenContainer = 2,
    UseObject     = 3,  ///< Lever, door, altar, etc.
    Examine       = 4,  ///< Inspect / tooltip server data.
    PickUp        = 5,  ///< Ground item loot request.
};

/// Schema v1 payload for OpCode::Interact.
struct Interact
{
    static constexpr OpCode        kOpCode        = OpCode::Interact;
    static constexpr std::uint16_t kSchemaVersion = 1;

    InteractKind  kind         = InteractKind::Default;
    std::uint32_t targetEntity = 0;
    std::uint32_t contextId    = 0;   ///< Opaque context (dialogue node id, container slot, ...).
    std::uint16_t optionIndex  = 0;   ///< Used by DialogueReply and similar.

    void Encode(ByteWriter& w) const
    {
        w.WriteHeader(kOpCode, kSchemaVersion);
        w.WriteU8(static_cast<std::uint8_t>(kind));
        w.WriteU32(targetEntity);
        w.WriteU32(contextId);
        w.WriteU16(optionIndex);
    }

    [[nodiscard]] static bool Decode(ByteReader& r, Interact& out)
    {
        std::uint16_t version = 0;
        if (!r.ReadHeader(kOpCode, version)) return false;
        if (version != kSchemaVersion)       return false;

        out.kind         = static_cast<InteractKind>(r.ReadU8());
        out.targetEntity = r.ReadU32();
        out.contextId    = r.ReadU32();
        out.optionIndex  = r.ReadU16();
        return r.Ok();
    }
};

} // namespace SagaEngine::Gameplay::Commands
