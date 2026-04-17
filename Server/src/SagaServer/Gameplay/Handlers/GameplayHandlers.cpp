/// @file GameplayHandlers.cpp
/// @brief Stub implementations for every gameplay command handler.
///
/// Each handler logs the decoded payload and returns success. Gameplay logic
/// is intentionally absent — this file is the hook-up point, not the rules
/// engine. Real handlers will call into CombatSystem / InventorySystem /
/// TradeSystem / ChatSystem once those exist.

#include "SagaServer/Gameplay/Handlers/GameplayHandlers.h"

#include "SagaEngine/Core/Log/Log.h"

namespace SagaServer::Gameplay::Handlers
{

namespace
{
constexpr const char* kTag = "GameplayHandler";
} // namespace

// ─── MoveRequest ──────────────────────────────────────────────────────

bool OnMoveRequest(std::uint64_t clientId, const E::MoveRequest& cmd,
                    std::vector<std::uint8_t>& /*out*/)
{
    SAGA_LOG_DEBUG(kTag,
        "MoveRequest client=%llu kind=%u target=%u pos=(%.2f,%.2f,%.2f) speed=%.2f cancel=%d",
        static_cast<unsigned long long>(clientId),
        static_cast<unsigned>(cmd.kind),
        cmd.targetEntity,
        cmd.targetX, cmd.targetY, cmd.targetZ,
        cmd.speedHint,
        cmd.cancel ? 1 : 0);
    // TODO: hand off to MovementSystem (path request / follow target / teleport gate).
    return true;
}

// ─── CastSpell ────────────────────────────────────────────────────────

bool OnCastSpell(std::uint64_t clientId, const E::CastSpell& cmd,
                  std::vector<std::uint8_t>& /*out*/)
{
    SAGA_LOG_DEBUG(kTag,
        "CastSpell client=%llu spell=%u target=%u pos=(%.2f,%.2f,%.2f) combo=%u",
        static_cast<unsigned long long>(clientId),
        cmd.spellId, cmd.targetEntity,
        cmd.targetX, cmd.targetY, cmd.targetZ,
        static_cast<unsigned>(cmd.comboFlags));
    // TODO: CombatSystem::BeginCast (GCD, mana, LoS, range, silence, cooldown).
    return true;
}

// ─── Interact ─────────────────────────────────────────────────────────

bool OnInteract(std::uint64_t clientId, const E::Interact& cmd,
                 std::vector<std::uint8_t>& /*out*/)
{
    SAGA_LOG_DEBUG(kTag,
        "Interact client=%llu kind=%u target=%u ctx=%u option=%u",
        static_cast<unsigned long long>(clientId),
        static_cast<unsigned>(cmd.kind),
        cmd.targetEntity, cmd.contextId,
        static_cast<unsigned>(cmd.optionIndex));
    // TODO: dispatch by target's component config (dialogue / vendor / container / object).
    return true;
}

// ─── Trade ────────────────────────────────────────────────────────────

bool OnTrade(std::uint64_t clientId, const E::Trade& cmd,
              std::vector<std::uint8_t>& /*out*/)
{
    SAGA_LOG_DEBUG(kTag,
        "Trade client=%llu verb=%u partner=%u slot=%u item=%u qty=%u gold=%llu",
        static_cast<unsigned long long>(clientId),
        static_cast<unsigned>(cmd.verb),
        cmd.partnerEntity,
        static_cast<unsigned>(cmd.slotIndex),
        cmd.itemEntity,
        cmd.quantity,
        static_cast<unsigned long long>(cmd.goldOffered));
    // TODO: TradeSystem verb handler + state-machine guard.
    return true;
}

// ─── UseItem ──────────────────────────────────────────────────────────

bool OnUseItem(std::uint64_t clientId, const E::UseItem& cmd,
                std::vector<std::uint8_t>& /*out*/)
{
    SAGA_LOG_DEBUG(kTag,
        "UseItem client=%llu item=%u slot=%u target=%u pos=(%.2f,%.2f,%.2f) charge=%u",
        static_cast<unsigned long long>(clientId),
        cmd.itemEntity,
        static_cast<unsigned>(cmd.inventorySlot),
        cmd.targetEntity,
        cmd.targetX, cmd.targetY, cmd.targetZ,
        static_cast<unsigned>(cmd.chargeIndex));
    // TODO: InventorySystem::UseItem (ownership, cooldown, charges, effect resolution).
    return true;
}

// ─── Equip ────────────────────────────────────────────────────────────

bool OnEquip(std::uint64_t clientId, const E::Equip& cmd,
              std::vector<std::uint8_t>& /*out*/)
{
    SAGA_LOG_DEBUG(kTag,
        "Equip client=%llu verb=%u item=%u slot=%u alt=%u",
        static_cast<unsigned long long>(clientId),
        static_cast<unsigned>(cmd.verb),
        cmd.itemEntity,
        static_cast<unsigned>(cmd.paperDollSlot),
        static_cast<unsigned>(cmd.altSlot));
    // TODO: InventorySystem::Equip (slot compat, requirements, bound flags).
    return true;
}

// ─── ChatMessage ──────────────────────────────────────────────────────

bool OnChatMessage(std::uint64_t clientId, const E::ChatMessage& cmd,
                    std::vector<std::uint8_t>& /*out*/)
{
    SAGA_LOG_DEBUG(kTag,
        "ChatMessage client=%llu ch=%u to=%u msgId=%u len=%zu",
        static_cast<unsigned long long>(clientId),
        static_cast<unsigned>(cmd.channel),
        cmd.recipientEntity,
        cmd.clientMsgId,
        cmd.text.size());
    // TODO: ChatSystem (permission, anti-spam, moderation, channel fan-out).
    return true;
}

// ─── Bulk registration ────────────────────────────────────────────────

void RegisterGameplayHandlerStubs(GameplayCommandDispatcher& dispatcher)
{
    dispatcher.RegisterHandler<E::MoveRequest>(&OnMoveRequest);
    dispatcher.RegisterHandler<E::CastSpell>  (&OnCastSpell);
    dispatcher.RegisterHandler<E::Interact>   (&OnInteract);
    dispatcher.RegisterHandler<E::Trade>      (&OnTrade);
    dispatcher.RegisterHandler<E::UseItem>    (&OnUseItem);
    dispatcher.RegisterHandler<E::Equip>      (&OnEquip);
    dispatcher.RegisterHandler<E::ChatMessage>(&OnChatMessage);

    SAGA_LOG_INFO(kTag, "Registered %zu gameplay handler stubs",
                  dispatcher.RegisteredCount());
}

} // namespace SagaServer::Gameplay::Handlers
