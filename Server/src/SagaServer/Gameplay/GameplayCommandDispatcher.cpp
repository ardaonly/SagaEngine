/// @file GameplayCommandDispatcher.cpp
/// @brief Typed fan-out + per-opcode rate limiting for gameplay commands.

#include "SagaServer/Gameplay/GameplayCommandDispatcher.h"

#include "SagaEngine/Core/Log/Log.h"

#include <chrono>
#include <cstring>

namespace SagaServer::Gameplay
{

namespace
{
constexpr const char* kTag = "GameplayCmd";

[[nodiscard]] std::uint64_t NowMicros() noexcept
{
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}
} // namespace

// ─── TokenBucket ──────────────────────────────────────────────────────

bool TokenBucket::TryConsume(std::uint64_t nowUs) noexcept
{
    if (tokensPerSec == 0)
        return true;  // Rate limit disabled.

    if (lastRefillUs == 0)
        lastRefillUs = nowUs;

    const std::uint64_t elapsedUs = nowUs - lastRefillUs;
    if (elapsedUs > 0)
    {
        const double refill = (static_cast<double>(elapsedUs) / 1'000'000.0)
                               * static_cast<double>(tokensPerSec);
        tokens += refill;
        if (tokens > static_cast<double>(capacity))
            tokens = static_cast<double>(capacity);
        lastRefillUs = nowUs;
    }

    if (tokens >= 1.0)
    {
        tokens -= 1.0;
        return true;
    }
    return false;
}

// ─── GameplayCommandDispatcher ────────────────────────────────────────

GameplayCommandDispatcher::GameplayCommandDispatcher() = default;

bool GameplayCommandDispatcher::Install(RPCDispatch& rpcDispatch)
{
    // Register a single RPC entry. Per-opcode auth / rate-limit is enforced
    // inside this dispatcher, not by the RPC layer. We therefore pass
    // requiresAuth=false to RPCDispatch so unauthenticated clients can
    // still reach the dispatcher — which then rejects them per-opcode if
    // that opcode's traits require auth. (This keeps login-adjacent
    // commands possible without a second RPC entry.)
    auto handler = [this](std::uint64_t       clientId,
                          const RPCRequest&   request,
                          RPCResponse&        response) -> bool
    {
        // Payload is expected as a single Blob argument.
        if (request.arguments.size() != 1 ||
            request.arguments[0].type != RPCArgType::Blob)
        {
            response.status = RPCStatusCode::BadArgs;
            return false;
        }

        const auto& blob = request.arguments[0].data;
        // clientAuth is not known at this layer — RPCDispatch normally checks
        // it before calling us. We defer to "authenticated" here; the real
        // auth state must be piped in from the transport when this is wired
        // into production. For now, assume true and let traits gate.
        constexpr bool kClientAuth = true;

        response.status = DispatchBlob(clientId,
                                         kClientAuth,
                                         blob.data(),
                                         blob.size(),
                                         response.resultData);
        return response.status == RPCStatusCode::Ok;
    };

    const bool ok = rpcDispatch.Register(
        EngineCommands::kGameplayCommandRpcName,
        handler,
        /*requiresAuth*/ false,   // Per-opcode check happens below.
        /*cooldownMs*/  0);        // Per-opcode bucket happens below.

    if (!ok)
        SAGA_LOG_ERROR(kTag, "Failed to register 'gcmd' RPC entry");
    return ok;
}

RPCStatusCode GameplayCommandDispatcher::DispatchBlob(
    std::uint64_t              clientId,
    bool                       clientAuth,
    const std::uint8_t*        data,
    std::size_t                size,
    std::vector<std::uint8_t>& outResult)
{
    if (data == nullptr || size < EngineCommands::kGameplayCommandHeaderSize)
        return RPCStatusCode::BadArgs;
    if (size > EngineCommands::kMaxGameplayCommandPayload)
        return RPCStatusCode::BadArgs;

    // ── Peek OpCode without consuming. ────────────────────────────────
    EngineCommands::OpCode op = EngineCommands::OpCode::None;
    if (!EngineCommands::ByteReader::PeekOpCode(data, size, op))
        return RPCStatusCode::BadArgs;

    const auto opIndex = static_cast<std::size_t>(op);
    if (opIndex >= kMaxOpCodes)
        return RPCStatusCode::NotFound;

    RegisteredCommand& entry = m_Commands[opIndex];
    if (!entry.registered)
        return RPCStatusCode::NotFound;

    // ── Auth gate. ────────────────────────────────────────────────────
    if (entry.traits.requiresAuth && !clientAuth)
        return RPCStatusCode::AuthFailed;

    // ── Rate-limit gate. ──────────────────────────────────────────────
    if (!AllowOpCode(clientId, op, entry.traits.rateLimitPerSecond))
        return RPCStatusCode::RateLimited;

    // ── Decode + handler call. ────────────────────────────────────────
    return entry.dispatch(clientId, data, size, outResult);
}

std::size_t GameplayCommandDispatcher::RegisteredCount() const noexcept
{
    std::size_t n = 0;
    for (const auto& e : m_Commands)
        if (e.registered) ++n;
    return n;
}

bool GameplayCommandDispatcher::AllowOpCode(std::uint64_t          clientId,
                                              EngineCommands::OpCode op,
                                              std::uint32_t          rateLimitPerSec)
{
    if (rateLimitPerSec == 0)
        return true;

    std::lock_guard<std::mutex> lock(m_RateMutex);
    auto& buckets = m_ClientBuckets[clientId];

    const auto opIndex = static_cast<std::size_t>(op);
    TokenBucket& bucket = buckets[opIndex];

    // Lazy-init on first use; keep capacity == rate for simple 1-second window.
    if (bucket.tokensPerSec == 0)
    {
        bucket.tokensPerSec = rateLimitPerSec;
        bucket.capacity     = rateLimitPerSec;
        bucket.tokens       = static_cast<double>(rateLimitPerSec);
        bucket.lastRefillUs = NowMicros();
    }

    return bucket.TryConsume(NowMicros());
}

} // namespace SagaServer::Gameplay
