/// @file CatastrophicRecoveryManager.cpp
/// @file Catastrophic recovery protocol for the replication pipeline.

#include "SagaEngine/Client/Replication/CatastrophicRecoveryManager.h"

#include "SagaEngine/Core/Log/Log.h"

#include <utility>

namespace SagaEngine::Client::Replication {

namespace {

constexpr const char* kLogTag = "Replication";

} // namespace

// ─── Configure ──────────────────────────────────────────────────────────────

void CatastrophicRecoveryManager::Configure(CatastrophicRecoveryConfig config) noexcept
{
    config_ = config;
}

void CatastrophicRecoveryManager::SetCallbacks(OnSoftResyncFn softResync,
                                                OnHardReconnectFn hardReconnect,
                                                OnGracefulDisconnectFn disconnect) noexcept
{
    onSoftResync_     = std::move(softResync);
    onHardReconnect_  = std::move(hardReconnect);
    onDisconnect_     = std::move(disconnect);
}

// ─── Success ────────────────────────────────────────────────────────────────

void CatastrophicRecoveryManager::RecordSuccess() noexcept
{
    if (state_ != RecoveryState::Idle)
    {
        LOG_INFO(kLogTag,
                 "CatastrophicRecovery: recovery succeeded — returning to Idle "
                 "(retries=%u, reconnects=%u)",
                 static_cast<unsigned>(snapshotRetryCount_),
                 static_cast<unsigned>(reconnectAttemptCount_));
    }

    snapshotRetryCount_ = 0;
    reconnectAttemptCount_ = 0;
    desyncCountInWindow_ = 0;
    state_ = RecoveryState::Idle;
}

// ─── Snapshot failure ───────────────────────────────────────────────────────

void CatastrophicRecoveryManager::RecordSnapshotFailure() noexcept
{
    snapshotRetryCount_++;

    if (state_ == RecoveryState::Idle)
        state_ = RecoveryState::SoftResyncing;

    if (snapshotRetryCount_ > config_.maxSnapshotRetries)
    {
        LOG_WARN(kLogTag,
                 "CatastrophicRecovery: %u snapshot retries exceeded — escalating to hard reconnect",
                 static_cast<unsigned>(snapshotRetryCount_));
        EscalateFromSoftResync();
    }
    else
    {
        LOG_WARN(kLogTag,
                 "CatastrophicRecovery: snapshot apply failure #%u — requesting retry",
                 static_cast<unsigned>(snapshotRetryCount_));
        if (onSoftResync_)
            onSoftResync_();
    }
}

// ─── Desync ─────────────────────────────────────────────────────────────────

void CatastrophicRecoveryManager::RecordDesync(std::uint64_t currentTick) noexcept
{
    // Start or update the sliding window.
    if (desyncCountInWindow_ == 0)
        desyncWindowStart_ = currentTick;

    // Expire old desyncs outside the window.
    if (currentTick - desyncWindowStart_ > config_.desyncWindowTicks)
    {
        desyncCountInWindow_ = 0;
        desyncWindowStart_ = currentTick;
    }

    desyncCountInWindow_++;

    if (desyncCountInWindow_ >= config_.desyncsBeforeHardReconnect)
    {
        LOG_WARN(kLogTag,
                 "CatastrophicRecovery: %u desyncs in %u ticks — escalating to hard reconnect",
                 static_cast<unsigned>(desyncCountInWindow_),
                 static_cast<unsigned>(config_.desyncWindowTicks));
        EscalateFromSoftResync();
    }
}

// ─── Tick ───────────────────────────────────────────────────────────────────

void CatastrophicRecoveryManager::Tick(std::uint64_t currentTick) noexcept
{
    (void)currentTick;

    // Time-based transitions can be added here if needed.
    // For now, escalation is event-driven (failure/desync counters).
}

// ─── Force escalate ─────────────────────────────────────────────────────────

void CatastrophicRecoveryManager::ForceEscalate() noexcept
{
    switch (state_)
    {
        case RecoveryState::Idle:
        case RecoveryState::SoftResyncing:
            EscalateFromSoftResync();
            break;

        case RecoveryState::HardReconnecting:
            EscalateFromHardReconnect();
            break;

        case RecoveryState::Disconnecting:
            // Already at the top — nothing to escalate to.
            break;
    }
}

// ─── Reset ──────────────────────────────────────────────────────────────────

void CatastrophicRecoveryManager::Reset() noexcept
{
    snapshotRetryCount_ = 0;
    reconnectAttemptCount_ = 0;
    desyncCountInWindow_ = 0;
    desyncWindowStart_ = 0;
    state_ = RecoveryState::Idle;
}

// ─── Internal escalation ────────────────────────────────────────────────────

void CatastrophicRecoveryManager::EscalateFromSoftResync() noexcept
{
    state_ = RecoveryState::HardReconnecting;
    reconnectAttemptCount_++;

    if (reconnectAttemptCount_ > config_.maxReconnectAttempts)
    {
        LOG_ERROR(kLogTag,
                  "CatastrophicRecovery: %u reconnect attempts exceeded — graceful disconnect",
                  static_cast<unsigned>(reconnectAttemptCount_));
        EscalateFromHardReconnect();
    }
    else
    {
        LOG_INFO(kLogTag,
                 "CatastrophicRecovery: hard reconnect attempt #%u",
                 static_cast<unsigned>(reconnectAttemptCount_));
        if (onHardReconnect_)
            onHardReconnect_();
    }
}

void CatastrophicRecoveryManager::EscalateFromHardReconnect() noexcept
{
    state_ = RecoveryState::Disconnecting;

    LOG_ERROR(kLogTag,
              "CatastrophicRecovery: all recovery attempts exhausted — disconnecting");

    if (onDisconnect_)
        onDisconnect_();
}

} // namespace SagaEngine::Client::Replication
