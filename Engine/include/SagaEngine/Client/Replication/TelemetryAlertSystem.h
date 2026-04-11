/// @file TelemetryAlertSystem.h
/// @brief Threshold-based alerting for replication telemetry.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Converts raw telemetry counters into actionable alerts so
///          operators are notified when replication health degrades.
///          Metrics without alerting are just decoration.
///
/// Design rules:
///   - Each alert has a threshold, a severity, and a cool-down period
///     to prevent alert storms.
///   - Alerts are evaluated every Tick() call — no background threads.
///   - Alert callbacks are invoked synchronously so the caller can
///     log, escalate, or trigger automatic recovery.

#pragma once

#include "SagaEngine/Client/Replication/ReplicationTelemetry.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>

namespace SagaEngine::Client::Replication {

// ─── Alert severity ────────────────────────────────────────────────────────

enum class AlertSeverity : std::uint8_t
{
    Info,       ///< Informational — no action required.
    Warning,    ///< Degradation detected — monitor closely.
    Critical,   ///< Immediate action required — desync imminent.
};

// ─── Alert definition ──────────────────────────────────────────────────────

/// A single alert rule evaluated against telemetry snapshots.
struct AlertRule
{
    /// Human-readable name for this alert.
    const char* name = nullptr;

    /// Severity level.
    AlertSeverity severity = AlertSeverity::Info;

    /// Threshold value that triggers this alert.
    std::uint64_t threshold = 0;

    /// Minimum ticks between consecutive firings (cool-down).
    std::uint32_t coolDownTicks = 60;

    /// Callback invoked when the threshold is breached.
    /// The callback receives the current telemetry snapshot.
    std::function<void(const ReplicationTelemetry&)> onBreach;
};

// ─── Alert state ────────────────────────────────────────────────────────────

struct AlertState
{
    bool                    active = false;
    std::uint64_t           lastBreachedAt = 0;
    std::uint32_t           fireCount = 0;
};

// ─── Telemetry alert system ────────────────────────────────────────────────

/// Evaluates alert rules against telemetry snapshots and fires callbacks
/// when thresholds are breached.
///
/// Thread-affine: all methods must be called from the game thread.
class TelemetryAlertSystem
{
public:
    TelemetryAlertSystem() = default;
    ~TelemetryAlertSystem() = default;

    TelemetryAlertSystem(const TelemetryAlertSystem&)            = delete;
    TelemetryAlertSystem& operator=(const TelemetryAlertSystem&) = delete;

    /// Maximum number of alert rules.
    static constexpr std::size_t kMaxAlertRules = 16;

    /// Register an alert rule.  Returns the rule index, or -1 if full.
    [[nodiscard]] int RegisterAlert(AlertRule rule) noexcept;

    /// Evaluate all registered alert rules against the current telemetry.
    /// Call once per frame after telemetry is updated.
    void Tick(std::uint64_t currentTick, const ReplicationTelemetry& telemetry) noexcept;

    /// Return the number of currently active alerts.
    [[nodiscard]] std::uint32_t ActiveAlertCount() const noexcept;

    /// Reset all alert state.
    void Reset() noexcept;

private:
    std::array<AlertRule, kMaxAlertRules> rules_{};
    std::array<AlertState, kMaxAlertRules> states_{};
    std::size_t ruleCount_ = 0;
};

} // namespace SagaEngine::Client::Replication
