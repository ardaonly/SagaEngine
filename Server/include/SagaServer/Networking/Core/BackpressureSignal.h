/// @file BackpressureSignal.h
/// @brief Backpressure signaling interface between network transport and replication.
///
/// Layer  : SagaServer / Networking / Core
/// Purpose: When the network transport layer's send queue fills beyond a
///          configurable threshold, the replication system must reduce its
///          output rate — either by dropping lower-priority entity updates or
///          by reducing the snapshot frequency.  BackpressureSignal is the
///          decoupled notification mechanism that bridges these two layers
///          without creating a direct dependency from transport → replication.
///
/// Design:
///   - A simple observer / callback pattern.
///   - The transport layer calls NotifyPressure() with a per-client pressure
///     level (0.0 = idle, 1.0 = at capacity).
///   - The replication layer registers a listener to adapt bandwidth budgets.
///   - Thread-safe: transport threads may call NotifyPressure() from IO threads
///     while the replication layer reads from the tick thread.

#pragma once

#include "SagaServer/Networking/Core/NetworkTypes.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace SagaEngine::Networking
{

// ─── Pressure Level ───────────────────────────────────────────────────────────

/// Per-client backpressure state snapshot.
struct ClientPressure
{
    ClientId clientId{INVALID_CLIENT_ID};
    float    level{0.0f};         ///< 0.0 = no pressure, 1.0 = fully saturated.
    uint32_t queuedBytes{0};      ///< Current bytes in the send queue.
    uint32_t queueCapacity{0};    ///< Max bytes the queue can hold.
};

// ─── Backpressure Thresholds ──────────────────────────────────────────────────

/// Thresholds that trigger graduated backpressure responses.
struct BackpressureConfig
{
    float warnThreshold{0.6f};    ///< Begin reducing low-priority replication.
    float criticalThreshold{0.85f};///< Drop all droppable updates.
    float emergencyThreshold{0.95f};///< Pause replication until pressure drops.
    uint32_t checkIntervalTicks{4};///< Re-evaluate pressure every N ticks.
};

// ─── Backpressure Signal ──────────────────────────────────────────────────────

/// Callback signature fired when a client's pressure crosses a threshold.
/// `level` is the raw pressure value in [0.0, 1.0].
using OnBackpressureFn = std::function<void(ClientId clientId, float level)>;

/// Central backpressure coordinator.
///
/// Ownership:
///   Created once by the ZoneServer and shared (by raw pointer) with both the
///   transport layer (writer) and the ReplicationManager (reader/listener).
class BackpressureSignal final
{
public:
    explicit BackpressureSignal(BackpressureConfig config = {});
    ~BackpressureSignal() = default;

    BackpressureSignal(const BackpressureSignal&)            = delete;
    BackpressureSignal& operator=(const BackpressureSignal&) = delete;

    // ── Transport side (writer) ───────────────────────────────────────────────

    /// Called by the transport layer whenever the send queue state changes.
    /// Safe to call from any IO thread.
    void NotifyPressure(ClientId clientId,
                        uint32_t queuedBytes,
                        uint32_t queueCapacity);

    // ── Replication side (reader) ─────────────────────────────────────────────

    /// Register a callback that fires when pressure crosses a threshold.
    /// Only one listener supported — setting a new one replaces the old.
    void SetListener(OnBackpressureFn fn);

    /// Query the last-known pressure level for a client.
    /// Returns 0.0 if the client is not tracked.
    [[nodiscard]] float GetPressure(ClientId clientId) const;

    /// Query the full state for a client.
    [[nodiscard]] ClientPressure GetClientPressure(ClientId clientId) const;

    /// True if the client's pressure exceeds the warn threshold.
    [[nodiscard]] bool IsUnderPressure(ClientId clientId) const;

    /// True if the client's pressure exceeds the emergency threshold.
    [[nodiscard]] bool IsEmergency(ClientId clientId) const;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /// Remove tracking for a disconnected client.
    void RemoveClient(ClientId clientId);

    /// Clear all tracked state.
    void Clear();

    // ── Config ────────────────────────────────────────────────────────────────

    void SetConfig(const BackpressureConfig& config) noexcept;
    [[nodiscard]] const BackpressureConfig& GetConfig() const noexcept { return m_config; }

private:
    mutable std::mutex                                  m_mutex;
    BackpressureConfig                                  m_config;
    std::unordered_map<ClientId, ClientPressure>        m_state;
    OnBackpressureFn                                    m_listener;
};

} // namespace SagaEngine::Networking
