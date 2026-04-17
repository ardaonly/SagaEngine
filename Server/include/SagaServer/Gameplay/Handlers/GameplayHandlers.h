/// @file GameplayHandlers.h
/// @brief Typed-handler stubs for every gameplay OpCode.
///
/// Layer  : SagaServer / Gameplay / Handlers
/// Purpose: One function per OpCode, matching the signature expected by
///          GameplayCommandDispatcher::RegisterHandler<T>. These are
///          *stubs only* — they validate input shape, log the attempt,
///          and return success. No gameplay logic lives here (no mana
///          check, no inventory state machine, no cooldown). That belongs
///          in dedicated gameplay systems (CombatSystem, InventorySystem,
///          TradeSystem, ChatSystem) which these handlers will call into.
///
/// Usage:
///   GameplayCommandDispatcher g;
///   g.Install(rpcDispatch);
///   RegisterGameplayHandlerStubs(g);

#pragma once

#include "SagaEngine/Gameplay/Commands/CastSpell.h"
#include "SagaEngine/Gameplay/Commands/ChatMessage.h"
#include "SagaEngine/Gameplay/Commands/Equip.h"
#include "SagaEngine/Gameplay/Commands/Interact.h"
#include "SagaEngine/Gameplay/Commands/MoveRequest.h"
#include "SagaEngine/Gameplay/Commands/Trade.h"
#include "SagaEngine/Gameplay/Commands/UseItem.h"
#include "SagaServer/Gameplay/GameplayCommandDispatcher.h"

#include <cstdint>
#include <vector>

namespace SagaServer::Gameplay::Handlers
{

namespace E = ::SagaEngine::Gameplay::Commands;

// ─── Typed stub declarations ──────────────────────────────────────────

bool OnMoveRequest (std::uint64_t clientId, const E::MoveRequest&  cmd, std::vector<std::uint8_t>& out);
bool OnCastSpell   (std::uint64_t clientId, const E::CastSpell&    cmd, std::vector<std::uint8_t>& out);
bool OnInteract    (std::uint64_t clientId, const E::Interact&     cmd, std::vector<std::uint8_t>& out);
bool OnTrade       (std::uint64_t clientId, const E::Trade&        cmd, std::vector<std::uint8_t>& out);
bool OnUseItem     (std::uint64_t clientId, const E::UseItem&      cmd, std::vector<std::uint8_t>& out);
bool OnEquip       (std::uint64_t clientId, const E::Equip&        cmd, std::vector<std::uint8_t>& out);
bool OnChatMessage (std::uint64_t clientId, const E::ChatMessage&  cmd, std::vector<std::uint8_t>& out);

// ─── Bulk registration helper ─────────────────────────────────────────

/// Register every stub handler on the dispatcher.
/// Boot-time call — replace individual handlers later with real systems.
void RegisterGameplayHandlerStubs(GameplayCommandDispatcher& dispatcher);

} // namespace SagaServer::Gameplay::Handlers
