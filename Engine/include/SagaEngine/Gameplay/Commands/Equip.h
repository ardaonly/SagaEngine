/// @file Equip.h
/// @brief Equip / unequip / swap an item into a paper-doll slot.
///
/// Layer  : SagaEngine / Gameplay / Commands
/// Purpose: A single verb covers equip, unequip and swap operations. The
///          server validates slot compatibility, level/class requirements,
///          durability and any "bound" flags.
///
/// Schema v1:
///   header(4) | verb(1) | itemEntity(4) | paperDollSlot(1) | altSlot(1)

#pragma once

#include "SagaEngine/Gameplay/Commands/CommandCodec.h"

#include <cstdint>

namespace SagaEngine::Gameplay::Commands
{

// ─── Equip ────────────────────────────────────────────────────────────

/// Paper-doll slot identifier. Values are stable; append-only.
enum class PaperDollSlot : std::uint8_t
{
    None       = 0,
    Head       = 1,
    Neck       = 2,
    Shoulders  = 3,
    Chest      = 4,
    Back       = 5,
    Wrist      = 6,
    Hands      = 7,
    Waist      = 8,
    Legs       = 9,
    Feet       = 10,
    Ring1      = 11,
    Ring2      = 12,
    Trinket1   = 13,
    Trinket2   = 14,
    MainHand   = 15,
    OffHand    = 16,
    TwoHand    = 17,
    Ranged     = 18,
    Ammo       = 19,
    Tabard     = 20,
    Shirt      = 21,
};

/// Action requested on the target slot.
enum class EquipVerb : std::uint8_t
{
    Equip   = 0,  ///< Move itemEntity from inventory → paperDollSlot.
    Unequip = 1,  ///< Move gear from paperDollSlot → inventory.
    Swap    = 2,  ///< Exchange paperDollSlot and altSlot contents.
};

/// Schema v1 payload for OpCode::Equip.
struct Equip
{
    static constexpr OpCode        kOpCode        = OpCode::Equip;
    static constexpr std::uint16_t kSchemaVersion = 1;

    EquipVerb     verb          = EquipVerb::Equip;
    std::uint32_t itemEntity    = 0;                      ///< Ignored for Unequip / Swap.
    PaperDollSlot paperDollSlot = PaperDollSlot::None;
    PaperDollSlot altSlot       = PaperDollSlot::None;    ///< Only used by Swap.

    void Encode(ByteWriter& w) const
    {
        w.WriteHeader(kOpCode, kSchemaVersion);
        w.WriteU8(static_cast<std::uint8_t>(verb));
        w.WriteU32(itemEntity);
        w.WriteU8(static_cast<std::uint8_t>(paperDollSlot));
        w.WriteU8(static_cast<std::uint8_t>(altSlot));
    }

    [[nodiscard]] static bool Decode(ByteReader& r, Equip& out)
    {
        std::uint16_t version = 0;
        if (!r.ReadHeader(kOpCode, version)) return false;
        if (version != kSchemaVersion)       return false;

        out.verb          = static_cast<EquipVerb>(r.ReadU8());
        out.itemEntity    = r.ReadU32();
        out.paperDollSlot = static_cast<PaperDollSlot>(r.ReadU8());
        out.altSlot       = static_cast<PaperDollSlot>(r.ReadU8());
        return r.Ok();
    }
};

} // namespace SagaEngine::Gameplay::Commands
