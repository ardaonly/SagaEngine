/// @file GameplayCommandDispatcher.h
/// @brief Fan-out from a single RPC entry ("gcmd") to typed gameplay handlers.
///
/// Layer  : SagaServer / Gameplay
/// Purpose: Gameplay commands arrive through the generic RPC system as a
///          single Blob argument under RPC name "gcmd". This dispatcher is
///          the middle layer that:
///            - Peeks the OpCode prefix,
///            - Applies per-opcode auth and rate-limit gates (from CommandTraits),
///            - Decodes the typed payload via the command's Decode(),
///            - Calls the registered typed handler.
///          The gameplay layer therefore never sees raw bytes.
///
/// Threading:
///   - Register*() are intended to run at boot on a single thread.
///   - Dispatch runs on whatever thread the transport hands RPCs on; all
///     mutable state (rate-limit buckets) is guarded by internal locks.

#pragma once

#include "SagaEngine/Gameplay/Commands/CommandCodec.h"
#include "SagaEngine/Gameplay/Commands/CommandTraits.h"
#include "SagaEngine/Gameplay/Commands/OpCode.h"
#include "SagaServer/Networking/Replication/RPC.h"

#include <array>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaServer::Gameplay
{

namespace EngineCommands = ::SagaEngine::Gameplay::Commands;

// ─── Typed handler signatures ─────────────────────────────────────────

/// Typed handler function — one per OpCode. Return true on success.
/// Output bytes in `outResult` are carried back to the client via the RPC
/// response payload; leave empty for fire-and-forget commands.
template <typename CommandT>
using TypedHandlerFn = std::function<bool(
    std::uint64_t          clientId,
    const CommandT&        cmd,
    std::vector<std::uint8_t>& outResult)>;

// ─── Internal registry entry ──────────────────────────────────────────

/// Type-erased registry entry — stores a decoder + handler bound against
/// the concrete command struct.
struct RegisteredCommand
{
    using DispatchFn = std::function<RPCStatusCode(
        std::uint64_t                   clientId,
        const std::uint8_t*             data,
        std::size_t                     size,
        std::vector<std::uint8_t>&      outResult)>;

    EngineCommands::CommandTraitsView traits;
    DispatchFn                        dispatch;
    bool                              registered = false;
};

// ─── Simple token bucket for per-opcode rate limiting ─────────────────

/// Minimal token-bucket rate limiter used per-(client, opcode).
/// Separate from RPCRateLimiter because commands share one RPC name; the
/// RPC-level limiter cannot discriminate between CastSpell and ChatMessage.
struct TokenBucket
{
    std::uint32_t capacity       = 1;   ///< Burst size.
    std::uint32_t tokensPerSec   = 1;   ///< Refill rate.
    double        tokens         = 1.0;
    std::uint64_t lastRefillUs   = 0;

    [[nodiscard]] bool TryConsume(std::uint64_t nowUs) noexcept;
};

// ─── Dispatcher ───────────────────────────────────────────────────────

/// Fans gameplay commands out by OpCode after gating on auth + rate-limit.
///
/// Typical wiring:
///   GameplayCommandDispatcher g;
///   g.Install(rpcDispatch);
///   g.RegisterHandler<CastSpell>(OnCastSpell);
///   g.RegisterHandler<MoveRequest>(OnMoveRequest);
///   ...
class GameplayCommandDispatcher
{
public:
    GameplayCommandDispatcher();
    ~GameplayCommandDispatcher() = default;

    GameplayCommandDispatcher(const GameplayCommandDispatcher&)            = delete;
    GameplayCommandDispatcher& operator=(const GameplayCommandDispatcher&) = delete;

    /// Wire the dispatcher into the RPC dispatch table under "gcmd".
    /// Must be called exactly once per RPCDispatch instance.
    bool Install(RPCDispatch& rpcDispatch);

    /// Register a typed handler for one OpCode. The OpCode is inferred from
    /// CommandT::kOpCode; CommandTraits<Op> provides auth / reliability /
    /// rate-limit at compile time.
    template <typename CommandT>
    bool RegisterHandler(TypedHandlerFn<CommandT> handler)
    {
        constexpr EngineCommands::OpCode op = CommandT::kOpCode;
        RegisteredCommand& entry = m_Commands[static_cast<std::size_t>(op)];

        if (entry.registered)
            return false;

        entry.traits     = EngineCommands::MakeTraitsView<op>();
        entry.registered = true;

        // Type-erase: capture decoder + typed handler.
        entry.dispatch = [h = std::move(handler)](
            std::uint64_t              clientId,
            const std::uint8_t*        data,
            std::size_t                size,
            std::vector<std::uint8_t>& outResult) -> RPCStatusCode
        {
            EngineCommands::ByteReader reader(data, size);
            CommandT cmd{};
            if (!CommandT::Decode(reader, cmd))
                return RPCStatusCode::BadArgs;

            return h(clientId, cmd, outResult)
                ? RPCStatusCode::Ok
                : RPCStatusCode::InternalError;
        };
        return true;
    }

    /// Direct entry point used by the RPC bridge callback. Exposed so
    /// tests can drive the dispatcher without going through RPCDispatch.
    RPCStatusCode DispatchBlob(std::uint64_t              clientId,
                                bool                       clientAuth,
                                const std::uint8_t*        data,
                                std::size_t                size,
                                std::vector<std::uint8_t>& outResult);

    /// Number of opcodes with a handler registered.
    [[nodiscard]] std::size_t RegisteredCount() const noexcept;

private:
    [[nodiscard]] bool AllowOpCode(std::uint64_t clientId,
                                     EngineCommands::OpCode op,
                                     std::uint32_t rateLimitPerSec);

    /// Fixed-size array indexed by OpCode. Sized generously for append-only
    /// opcode growth without rehashing.
    static constexpr std::size_t kMaxOpCodes = 256;
    std::array<RegisteredCommand, kMaxOpCodes> m_Commands;

    mutable std::mutex m_RateMutex;
    std::unordered_map<std::uint64_t,
        std::array<TokenBucket, kMaxOpCodes>>  m_ClientBuckets;
};

} // namespace SagaServer::Gameplay
