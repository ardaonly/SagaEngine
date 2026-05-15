/// @file UseItem.h
/// @brief Use a consumable, scroll, or otherwise "activate" an inventory item.
///
/// Layer  : SagaEngine / Gameplay / Commands
/// Purpose: Client requests activation of an inventory item. Server resolves
///          effect (heal, buff, summon, gate, open container, ...) from the
///          item's component configuration.
///
/// Schema v1:
///   header(4) | itemEntity(4) | inventorySlot(2) | targetEntity(4)
///           | targetX(4) | targetY(4) | targetZ(4) | chargeIndex(1)

#pragma once

#include "SagaEngine/Gameplay/Commands/CommandCodec.h"

#include <cstdint>

namespace SagaEngine::Gameplay::Commands
{

// ─── UseItem ──────────────────────────────────────────────────────────

/// Schema v1 payload for OpCode::UseItem.
struct UseItem
{
    static constexpr OpCode        kOpCode        = OpCode::UseItem;
    static constexpr std::uint16_t kSchemaVersion = 1;

    std::uint32_t itemEntity    = 0;      ///< Concrete inventory item entity id.
    std::uint16_t inventorySlot = 0;      ///< Redundant hint; server authoritative.
    std::uint32_t targetEntity  = 0;      ///< 0 for self-use or ground-targeted.
    float         targetX       = 0.0f;
    float         targetY       = 0.0f;
    float         targetZ       = 0.0f;
    std::uint8_t  chargeIndex   = 0;      ///< Multi-charge items (wands / scrolls).

    void Encode(ByteWriter& w) const
    {
        w.WriteHeader(kOpCode, kSchemaVersion);
        w.WriteU32(itemEntity);
        w.WriteU16(inventorySlot);
        w.WriteU32(targetEntity);
        w.WriteFloat(targetX);
        w.WriteFloat(targetY);
        w.WriteFloat(targetZ);
        w.WriteU8(chargeIndex);
    }

    [[nodiscard]] static bool Decode(ByteReader& r, UseItem& out)
    {
        std::uint16_t version = 0;
        if (!r.ReadHeader(kOpCode, version)) return false;
        if (version != kSchemaVersion)       return false;

        out.itemEntity    = r.ReadU32();
        out.inventorySlot = r.ReadU16();
        out.targetEntity  = r.ReadU32();
        out.targetX       = r.ReadFloat();
        out.targetY       = r.ReadFloat();
        out.targetZ       = r.ReadFloat();
        out.chargeIndex   = r.ReadU8();
        return r.Ok();
    }
};

} // namespace SagaEngine::Gameplay::Commands
