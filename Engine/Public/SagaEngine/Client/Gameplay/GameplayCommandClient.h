/// @file GameplayCommandClient.h
/// @brief Type-safe client-side stubs for every gameplay command.
///
/// Layer  : SagaEngine / Client / Gameplay
/// Purpose: The inverse of the server dispatcher: client code calls
///          `client.CastSpell(spellId, target)` and this class encodes the
///          command into a gameplay payload and hands it to the caller's
///          send function (typically the RPC client under "gcmd").
///
/// Design rules:
///   - No transport coupling. The client takes a std::function send hook
///     so unit tests and replays can substitute it.
///   - Reliability info travels alongside each send — the transport layer
///     (RPC client, reliable channel, transport backend) is free to honour
///     or ignore it, but we always pass it. This keeps the contract
///     explicit; no "which channel?" guessing later.
///   - No gameplay state on the client stub. It is a pure encoder.

#pragma once

#include "SagaEngine/Gameplay/Commands/CastSpell.h"
#include "SagaEngine/Gameplay/Commands/ChatMessage.h"
#include "SagaEngine/Gameplay/Commands/CommandTraits.h"
#include "SagaEngine/Gameplay/Commands/Equip.h"
#include "SagaEngine/Gameplay/Commands/Interact.h"
#include "SagaEngine/Gameplay/Commands/MoveRequest.h"
#include "SagaEngine/Gameplay/Commands/OpCode.h"
#include "SagaEngine/Gameplay/Commands/Trade.h"
#include "SagaEngine/Gameplay/Commands/UseItem.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace SagaEngine::Client::Gameplay
{

namespace Cmd = ::SagaEngine::Gameplay::Commands;

// ─── SendContext ──────────────────────────────────────────────────────

/// Metadata the transport layer needs in order to enqueue a gameplay
/// command on the right channel. Populated from CommandTraits at the
/// call site so the transport never has to look up the opcode itself.
struct SendContext
{
    Cmd::OpCode         opCode      = Cmd::OpCode::None;
    Cmd::Reliability    reliability = Cmd::Reliability::Reliable;
    bool                requiresAuth = true;
    const char*         debugName   = "Unknown";
};

/// Send function signature the client stub uses to hand encoded bytes off
/// to the transport. Return false if the transport could not enqueue
/// (backpressure, connection not ready, etc.).
using SendFn = std::function<bool(
    const SendContext&             ctx,
    const std::uint8_t*            payload,
    std::size_t                    payloadSize)>;

// ─── GameplayCommandClient ────────────────────────────────────────────

/// Client-side typed encoder. Holds a transport send hook and exposes
/// one method per gameplay command. Each method:
///   1. Encodes the command via its own Encode().
///   2. Looks up compile-time traits.
///   3. Calls the send hook with a populated SendContext.
class GameplayCommandClient
{
public:
    explicit GameplayCommandClient(SendFn sendFn)
        : m_Send(std::move(sendFn)) {}

    /// Replace the transport send hook (e.g. after reconnect).
    void SetSendFn(SendFn sendFn) { m_Send = std::move(sendFn); }

    // ── Typed sends ────────────────────────────────────────────────

    [[nodiscard]] bool SendMoveRequest(const Cmd::MoveRequest& cmd)
    { return SendTyped<Cmd::OpCode::MoveRequest>(cmd); }

    [[nodiscard]] bool SendCastSpell(const Cmd::CastSpell& cmd)
    { return SendTyped<Cmd::OpCode::CastSpell>(cmd); }

    [[nodiscard]] bool SendInteract(const Cmd::Interact& cmd)
    { return SendTyped<Cmd::OpCode::Interact>(cmd); }

    [[nodiscard]] bool SendTrade(const Cmd::Trade& cmd)
    { return SendTyped<Cmd::OpCode::Trade>(cmd); }

    [[nodiscard]] bool SendUseItem(const Cmd::UseItem& cmd)
    { return SendTyped<Cmd::OpCode::UseItem>(cmd); }

    [[nodiscard]] bool SendEquip(const Cmd::Equip& cmd)
    { return SendTyped<Cmd::OpCode::Equip>(cmd); }

    [[nodiscard]] bool SendChatMessage(const Cmd::ChatMessage& cmd)
    { return SendTyped<Cmd::OpCode::ChatMessage>(cmd); }

    // ── Ergonomic overloads ────────────────────────────────────────

    /// Shortcut: single-target spell.
    [[nodiscard]] bool CastSpell(std::uint32_t spellId, std::uint32_t targetEntity,
                                  std::uint64_t clientCastStartMs = 0,
                                  std::uint8_t  comboFlags        = 0)
    {
        Cmd::CastSpell c;
        c.spellId           = spellId;
        c.targetEntity      = targetEntity;
        c.clientCastStartMs = clientCastStartMs;
        c.comboFlags        = comboFlags;
        return SendCastSpell(c);
    }

    /// Shortcut: path to point.
    [[nodiscard]] bool MoveToPoint(float x, float y, float z, float speedHint = 0.0f)
    {
        Cmd::MoveRequest c;
        c.kind      = Cmd::MoveRequestKind::MoveToPoint;
        c.targetX   = x;
        c.targetY   = y;
        c.targetZ   = z;
        c.speedHint = speedHint;
        return SendMoveRequest(c);
    }

    /// Shortcut: default interact with target entity.
    [[nodiscard]] bool InteractWith(std::uint32_t targetEntity)
    {
        Cmd::Interact c;
        c.kind         = Cmd::InteractKind::Default;
        c.targetEntity = targetEntity;
        return SendInteract(c);
    }

    /// Shortcut: say on a channel.
    [[nodiscard]] bool Say(Cmd::ChatChannel channel, std::string text,
                            std::uint32_t recipientEntity = 0,
                            std::uint32_t clientMsgId     = 0)
    {
        Cmd::ChatMessage c;
        c.channel         = channel;
        c.recipientEntity = recipientEntity;
        c.clientMsgId     = clientMsgId;
        c.text            = std::move(text);
        return SendChatMessage(c);
    }

private:
    template <Cmd::OpCode Op, typename CommandT>
    [[nodiscard]] bool SendTyped(const CommandT& cmd)
    {
        static_assert(CommandT::kOpCode == Op, "OpCode / command type mismatch");

        if (!m_Send) return false;

        m_Buffer.clear();
        Cmd::ByteWriter w(m_Buffer);
        cmd.Encode(w);

        if (m_Buffer.size() > Cmd::kMaxGameplayCommandPayload)
            return false;

        const SendContext ctx{
            Op,
            Cmd::CommandTraits<Op>::reliability,
            Cmd::CommandTraits<Op>::requiresAuth,
            Cmd::CommandTraits<Op>::debugName,
        };
        return m_Send(ctx, m_Buffer.data(), m_Buffer.size());
    }

    SendFn                     m_Send;
    std::vector<std::uint8_t>  m_Buffer;  ///< Reused encode buffer (not thread-safe).
};

} // namespace SagaEngine::Client::Gameplay
