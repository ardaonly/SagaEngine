/// @file RateLimiter.h
/// @brief Per-connection token-bucket rate limiter for packet count and bandwidth.
///
/// Layer  : SagaServer / Networking / Core
/// Purpose: Defends the server tick loop against flooding by bounding how
///          many packets and how many bytes a single client may deliver per
///          second. Two independent token buckets run in parallel for each
///          tracked ClientId:
///            - Packet bucket  : cap on packets-per-second
///            - Byte bucket    : cap on bytes-per-second
///          A request consumes tokens from both buckets and is accepted only
///          when both have sufficient capacity. Repeated violations push the
///          client into a temporary quarantine state that the connection
///          manager can act upon (warn, disconnect, ban).
///
/// Threading:
///   - Buckets are stored in a single map guarded by a shared_mutex.
///   - The hot path (Allow) acquires a shared lock for lookup and then a
///     fine-grained spin on the per-bucket state. Creation of new buckets
///     upgrades to an exclusive lock.

#pragma once

#include "SagaServer/Networking/Core/NetworkTypes.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Networking
{

// ─── Configuration ────────────────────────────────────────────────────────────

/// Default token-bucket limits applied to every newly tracked client.
struct RateLimiterConfig
{
    float  packetsPerSecond{240.0f};     ///< Steady-state cap on packet rate.
    float  bytesPerSecond{256 * 1024.0f};///< Steady-state cap on byte rate.
    float  packetBurst{480.0f};          ///< Short-term bucket capacity (packets).
    float  bytesBurst{1024 * 1024.0f};   ///< Short-term bucket capacity (bytes).
    std::uint32_t maxViolationsBeforeQuarantine{64};
    std::chrono::milliseconds quarantineDuration{5000};
    std::chrono::milliseconds violationDecayInterval{2000};
};

// ─── Per-Bucket Statistics ────────────────────────────────────────────────────

/// Snapshot of a single client's bucket state.
struct RateLimiterStats
{
    float        packetTokens{0.0f};
    float        byteTokens{0.0f};
    std::uint64_t allowedPackets{0};
    std::uint64_t allowedBytes{0};
    std::uint64_t rejectedPackets{0};
    std::uint64_t rejectedBytes{0};
    std::uint32_t violations{0};
    bool          quarantined{false};
    std::uint64_t lastUpdateUnixMs{0};
};

// ─── RateLimiter ──────────────────────────────────────────────────────────────

/// Token-bucket rate limiter keyed by ClientId.
class RateLimiter final
{
public:
    explicit RateLimiter(RateLimiterConfig config = {});
    ~RateLimiter() = default;

    RateLimiter(const RateLimiter&)            = delete;
    RateLimiter& operator=(const RateLimiter&) = delete;

    /// Test whether `clientId` may send `packetBytes`. Creates the bucket on
    /// first use. Returns true when the request fits inside both buckets.
    [[nodiscard]] bool Allow(ClientId clientId, std::size_t packetBytes);

    /// Stop tracking a client and release its bucket.
    void ReleaseClient(ClientId clientId);

    /// Clear every bucket without touching the configuration.
    void Clear();

    /// True while the client is serving a quarantine cooldown.
    [[nodiscard]] bool IsQuarantined(ClientId clientId) const;

    /// Force a client out of quarantine and restore its violation counter.
    void ClearViolations(ClientId clientId);

    [[nodiscard]] RateLimiterStats GetStatistics(ClientId clientId) const;
    [[nodiscard]] std::vector<ClientId> ListTracked() const;
    [[nodiscard]] const RateLimiterConfig& GetConfig() const noexcept { return m_Config; }

private:
    // ── Internal bucket ──────────────────────────────────────────────────────

    struct Bucket
    {
        float         packetTokens{0.0f};
        float         byteTokens{0.0f};
        std::chrono::steady_clock::time_point lastRefill;
        std::chrono::steady_clock::time_point quarantineUntil;
        std::chrono::steady_clock::time_point lastViolationDecay;
        std::uint64_t allowedPackets{0};
        std::uint64_t allowedBytes{0};
        std::uint64_t rejectedPackets{0};
        std::uint64_t rejectedBytes{0};
        std::uint32_t violations{0};
        mutable std::mutex mutex;
    };

    /// Locate or create the bucket for `clientId`.
    Bucket* FetchBucket(ClientId clientId);

    /// Refill tokens based on elapsed time. Caller must hold bucket.mutex.
    void RefillLocked(Bucket& bucket, std::chrono::steady_clock::time_point now) const;

    /// Decay accumulated violations if enough wall-clock time has passed.
    void DecayViolationsLocked(Bucket& bucket, std::chrono::steady_clock::time_point now) const;

    RateLimiterConfig m_Config;

    mutable std::shared_mutex                                  m_Mutex;
    std::unordered_map<ClientId, std::unique_ptr<Bucket>>      m_Buckets;
};

} // namespace SagaEngine::Networking
