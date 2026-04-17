/// @file Trade.h
/// @brief Player-to-player trade state-machine verbs.
///
/// Layer  : SagaEngine / Gameplay / Commands
/// Purpose: Carries a single verb in the trade protocol. The full trade
///          lifecycle (invite → window open → offers → locks → accept →
///          escrow → commit) is owned by the server; the client only emits
///          verbs and observes replicated trade state.
///
/// Schema v1:
///   header(4) | verb(1) | partnerEntity(4) | slotIndex(2) | itemEntity(4)
///           | quantity(4) | goldOffered(8)

#pragma once

#include "SagaEngine/Gameplay/Commands/CommandCodec.h"

#include <cstdint>

namespace SagaEngine::Gameplay::Commands
{

// ─── Trade ────────────────────────────────────────────────────────────

/// Trade state-machine verb. Server validates preconditions for each.
enum class TradeVerb : std::uint8_t
{
    Invite     = 0,  ///< Request to open a trade window with partnerEntity.
    Accept     = 1,  ///< Accept a pending trade invite.
    Decline    = 2,  ///< Decline / cancel invite.
    PutItem    = 3,  ///< Place item into trade window slot.
    RemoveItem = 4,  ///< Remove item from trade window slot.
    SetGold    = 5,  ///< Offer an amount of gold.
    Lock       = 6,  ///< Lock the offer (pre-commit).
    Unlock     = 7,  ///< Unlock after edit.
    Confirm    = 8,  ///< Final commit after both sides locked.
    Cancel     = 9,  ///< Cancel and close the trade window.
};

/// Schema v1 payload for OpCode::Trade.
struct Trade
{
    static constexpr OpCode        kOpCode        = OpCode::Trade;
    static constexpr std::uint16_t kSchemaVersion = 1;

    TradeVerb     verb          = TradeVerb::Cancel;
    std::uint32_t partnerEntity = 0;
    std::uint16_t slotIndex     = 0;   ///< Trade window slot for PutItem / RemoveItem.
    std::uint32_t itemEntity    = 0;   ///< Inventory item entity id.
    std::uint32_t quantity      = 0;
    std::uint64_t goldOffered   = 0;

    void Encode(ByteWriter& w) const
    {
        w.WriteHeader(kOpCode, kSchemaVersion);
        w.WriteU8(static_cast<std::uint8_t>(verb));
        w.WriteU32(partnerEntity);
        w.WriteU16(slotIndex);
        w.WriteU32(itemEntity);
        w.WriteU32(quantity);
        w.WriteU64(goldOffered);
    }

    [[nodiscard]] static bool Decode(ByteReader& r, Trade& out)
    {
        std::uint16_t version = 0;
        if (!r.ReadHeader(kOpCode, version)) return false;
        if (version != kSchemaVersion)       return false;

        out.verb          = static_cast<TradeVerb>(r.ReadU8());
        out.partnerEntity = r.ReadU32();
        out.slotIndex     = r.ReadU16();
        out.itemEntity    = r.ReadU32();
        out.quantity      = r.ReadU32();
        out.goldOffered   = r.ReadU64();
        return r.Ok();
    }
};

} // namespace SagaEngine::Gameplay::Commands
