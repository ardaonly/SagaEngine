/// @file PacketRegistry.h
/// @brief Thread-safe dispatch table mapping PacketType values to server-side handlers.
///
/// Layer  : SagaServer / Networking / Core
/// Purpose: Every packet that reaches the server tick loop funnels through a
///          single dispatch point. PacketRegistry owns the handler lookup
///          table, enforces handler ownership, tracks per-type statistics,
///          and applies optional pre-dispatch validators (rate limits, size
///          checks, authorization gates). Handlers never see raw sockets —
///          they receive a stable PacketContext with the originating client,
///          packet header, and trimmed payload view.
///
/// Threading:
///   - Register/Unregister acquire an exclusive lock; these are setup-time
///     operations and are not expected on the hot path.
///   - Dispatch acquires only a shared lock to resolve the handler and then
///     releases it before invoking user code so handlers may re-enter the
///     registry (e.g. to register follow-up handlers during bootstrap).

#pragma once

#include "SagaServer/Networking/Core/NetworkTypes.h"
#include "SagaServer/Networking/Core/Packet.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Networking
{

// ─── PacketContext ────────────────────────────────────────────────────────────

/// Immutable view of a single inbound packet passed to every handler.
struct PacketContext
{
    ClientId        clientId{0};     ///< Sender identity assigned by ConnectionManager.
    PacketType      type{PacketType::Invalid};
    const uint8_t*  payload{nullptr};///< Pointer into the inbound ring buffer (not owned).
    std::size_t     payloadSize{0};
    std::uint64_t   serverTick{0};   ///< Tick at which dispatch occurred.
    std::uint64_t   recvTimeUnixMs{0};
};

// ─── Handler Signature ────────────────────────────────────────────────────────

/// Result emitted by a packet handler.
enum class PacketHandleResult : std::uint8_t
{
    Ok          = 0, ///< Handler consumed the packet successfully.
    Rejected    = 1, ///< Handler refused the packet for a non-fatal reason.
    MalformedIn = 2, ///< Payload failed decoding and should be logged.
    Throttled   = 3, ///< Handler was skipped because the client is rate-limited.
    Unknown     = 4  ///< No handler was registered for the packet type.
};

/// Function signature all packet handlers must match.
using PacketHandlerFn = std::function<PacketHandleResult(const PacketContext&)>;

/// Optional gate evaluated before a handler runs. Returning false short-circuits
/// dispatch with PacketHandleResult::Rejected and bumps the rejection counter.
using PacketValidatorFn = std::function<bool(const PacketContext&)>;

// ─── Statistics ───────────────────────────────────────────────────────────────

/// Per-packet-type counters maintained across dispatch calls.
struct PacketTypeStats
{
    std::uint64_t received{0};
    std::uint64_t handled{0};
    std::uint64_t rejected{0};
    std::uint64_t malformed{0};
    std::uint64_t throttled{0};
    std::uint64_t unknownHandler{0};
    std::uint64_t totalBytes{0};
    std::uint64_t lastSeenUnixMs{0};
};

/// Aggregate registry statistics covering every packet type.
struct PacketRegistryStats
{
    std::uint64_t totalReceived{0};
    std::uint64_t totalHandled{0};
    std::uint64_t totalRejected{0};
    std::uint64_t totalMalformed{0};
    std::uint64_t totalThrottled{0};
    std::uint64_t totalUnknown{0};
    std::uint64_t totalBytes{0};
    std::size_t   registeredTypes{0};
};

// ─── Configuration ────────────────────────────────────────────────────────────

/// Construction-time tuning for PacketRegistry.
struct PacketRegistryConfig
{
    std::size_t maxPayloadBytes{64 * 1024}; ///< Hard cap on any single packet.
    bool        trackPerTypeStats{true};    ///< Disable to skip stat bookkeeping on hot path.
    bool        logUnknownTypes{true};      ///< LOG_WARN on first unknown type per client.
};

// ─── PacketRegistry ───────────────────────────────────────────────────────────

/// Dispatch table for inbound server packets.
class PacketRegistry final
{
public:
    explicit PacketRegistry(PacketRegistryConfig config = {});
    ~PacketRegistry() = default;

    PacketRegistry(const PacketRegistry&)            = delete;
    PacketRegistry& operator=(const PacketRegistry&) = delete;

    // ── Handler management ───────────────────────────────────────────────────

    /// Register a handler for `type`. Replaces any previously registered handler.
    void RegisterHandler(PacketType type, PacketHandlerFn handler);

    /// Remove a previously registered handler. No-op if none was registered.
    void UnregisterHandler(PacketType type);

    /// Attach a validator fired before the handler. Multiple validators stack
    /// in registration order; the first one returning false aborts dispatch.
    void AddValidator(PacketType type, PacketValidatorFn validator);

    /// Remove every validator for `type`.
    void ClearValidators(PacketType type);

    /// Install a fallback handler invoked when no explicit handler matches.
    void SetFallbackHandler(PacketHandlerFn handler);

    [[nodiscard]] bool HasHandler(PacketType type) const;

    // ── Dispatch ─────────────────────────────────────────────────────────────

    /// Resolve and invoke the handler registered for `ctx.type`. Safe to call
    /// concurrently from any thread that owns `ctx` for the duration of the call.
    PacketHandleResult Dispatch(const PacketContext& ctx);

    /// Bulk dispatch — returns the number of packets consumed with Ok.
    std::size_t DispatchBatch(const PacketContext* contexts, std::size_t count);

    // ── Introspection ────────────────────────────────────────────────────────

    [[nodiscard]] PacketRegistryStats GetStatistics() const;
    [[nodiscard]] std::optional<PacketTypeStats> GetTypeStatistics(PacketType type) const;

    /// Reset every counter to zero. Registered handlers are preserved.
    void ResetStatistics();

    [[nodiscard]] const PacketRegistryConfig& GetConfig() const noexcept { return m_Config; }

private:
    // ── Internal types ───────────────────────────────────────────────────────

    struct Entry
    {
        PacketHandlerFn                 handler;
        std::vector<PacketValidatorFn>  validators;
    };

    /// Validate envelope invariants before touching the handler table.
    [[nodiscard]] bool ValidateEnvelope(const PacketContext& ctx,
                                        PacketHandleResult&  earlyResult) const;

    /// Update statistics for `type` under the dispatch lock discipline.
    void BumpStatistics(PacketType         type,
                        std::size_t        bytes,
                        PacketHandleResult result,
                        std::uint64_t      nowUnixMs);

    PacketRegistryConfig m_Config;

    mutable std::shared_mutex                    m_Mutex;
    std::unordered_map<PacketType, Entry>        m_Handlers;
    PacketHandlerFn                              m_FallbackHandler;

    mutable std::mutex                           m_StatsMutex;
    std::unordered_map<PacketType, PacketTypeStats> m_PerTypeStats;
    PacketRegistryStats                          m_AggregateStats;
};

} // namespace SagaEngine::Networking
