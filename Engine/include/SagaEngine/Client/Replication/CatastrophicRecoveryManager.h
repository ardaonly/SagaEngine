/// @file CatastrophicRecoveryManager.h
/// @file Catastrophic recovery protocol for the replication pipeline.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Handles the case where a full snapshot also fails to apply,
///          or where repeated desyncs indicate a deeper problem than a
///          simple packet loss.  Defines the escalation ladder from
///          soft resync through hard reconnect to graceful disconnect.
///
/// Escalation ladder:
///   1. Soft resync: request another full snapshot (retry up to N times).
///   2. Hard reconnect: tear down and re-establish the network connection.
///   3. Graceful disconnect: notify the user and return to the main menu.
///
/// The manager tracks consecutive failures and advances the escalation
/// state automatically when thresholds are exceeded.

#pragma once

#include "SagaEngine/Client/Replication/ReplicationStateMachine.h"

#include <cstdint>
#include <functional>

namespace SagaEngine::Client::Replication {

// ─── Recovery config ───────────────────────────────────────────────────────

struct CatastrophicRecoveryConfig
{
    /// Maximum number of consecutive full-snapshot apply failures
    /// before escalating to hard reconnect.
    std::uint32_t maxSnapshotRetries = 3;

    /// Maximum number of consecutive hard reconnect failures before
    /// escalating to graceful disconnect.
    std::uint32_t maxReconnectAttempts = 5;

    /// Number of consecutive desyncs within a sliding window that
    /// triggers a hard reconnect instead of a soft resync.
    std::uint32_t desyncsBeforeHardReconnect = 5;

    /// Sliding window size (in ticks) for counting consecutive desyncs.
    std::uint32_t desyncWindowTicks = 600;  // ~10 seconds at 60 Hz.
};

// ─── Recovery state ─────────────────────────────────────────────────────────

enum class RecoveryState : std::uint8_t
{
    Idle,               ///< No active recovery — normal operation.
    SoftResyncing,      ///< Requesting full snapshot retries.
    HardReconnecting,   ///< Tearing down and re-establishing connection.
    Disconnecting,      ///< Notifying user and returning to main menu.
};

// ─── Callbacks ──────────────────────────────────────────────────────────────

using OnSoftResyncFn = std::function<void()>;            ///< Request a full snapshot.
using OnHardReconnectFn = std::function<void()>;         ///< Re-establish network connection.
using OnGracefulDisconnectFn = std::function<void()>;    ///< Disconnect and notify user.

// ─── Catastrophic recovery manager ──────────────────────────────────────────

/// Manages the escalation ladder when recovery attempts fail.
class CatastrophicRecoveryManager
{
public:
    CatastrophicRecoveryManager() = default;
    ~CatastrophicRecoveryManager() = default;

    CatastrophicRecoveryManager(const CatastrophicRecoveryManager&)            = delete;
    CatastrophicRecoveryManager& operator=(const CatastrophicRecoveryManager&) = delete;

    /// Configure the manager.  Must be called before use.
    void Configure(CatastrophicRecoveryConfig config) noexcept;

    /// Install callbacks for each escalation level.
    void SetCallbacks(OnSoftResyncFn softResync,
                      OnHardReconnectFn hardReconnect,
                      OnGracefulDisconnectFn disconnect) noexcept;

    /// Record a successful recovery (snapshot applied cleanly).
    /// Resets all failure counters.
    void RecordSuccess() noexcept;

    /// Record a failed snapshot apply.  May trigger escalation.
    void RecordSnapshotFailure() noexcept;

    /// Record a desync event.  May trigger hard reconnect if the
    /// rate exceeds the configured threshold.
    void RecordDesync(std::uint64_t currentTick) noexcept;

    /// Tick the recovery manager.  Advances timed transitions.
    void Tick(std::uint64_t currentTick) noexcept;

    /// Return the current recovery state.
    [[nodiscard]] RecoveryState GetState() const noexcept { return state_; }

    /// Return the number of consecutive snapshot retries so far.
    [[nodiscard]] std::uint32_t GetRetryCount() const noexcept { return snapshotRetryCount_; }

    /// Return the number of consecutive reconnect attempts so far.
    [[nodiscard]] std::uint32_t GetReconnectCount() const noexcept { return reconnectAttemptCount_; }

    /// Force escalation to the next level.  Used when the caller
    /// determines that the current level is unrecoverable.
    void ForceEscalate() noexcept;

    /// Reset to Idle.  Call after a successful recovery or manual reset.
    void Reset() noexcept;

private:
    CatastrophicRecoveryConfig config_{};
    RecoveryState state_ = RecoveryState::Idle;

    std::uint32_t snapshotRetryCount_ = 0;
    std::uint32_t reconnectAttemptCount_ = 0;
    std::uint32_t desyncCountInWindow_ = 0;
    std::uint64_t desyncWindowStart_ = 0;

    OnSoftResyncFn           onSoftResync_;
    OnHardReconnectFn        onHardReconnect_;
    OnGracefulDisconnectFn   onDisconnect_;

    void EscalateFromSoftResync() noexcept;
    void EscalateFromHardReconnect() noexcept;
};

} // namespace SagaEngine::Client::Replication
