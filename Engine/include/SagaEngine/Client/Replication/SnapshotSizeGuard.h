/// @file SnapshotSizeGuard.h
/// @file Snapshot size enforcement for the replication pipeline.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Enforces hard limits on snapshot sizes to protect the client
///          from malformed or malicious payloads that would otherwise
///          exhaust memory or cause allocator storms.  The guard operates
///          at the wire decoder boundary — before any bytes reach the ECS.

#pragma once

#include <cstdint>

namespace SagaEngine::Client::Replication {

// ─── Size guard config ─────────────────────────────────────────────────────

struct SnapshotSizeConfig
{
    /// Maximum allowed full snapshot payload in bytes.  Snapshots
    /// exceeding this limit are rejected before decoding.
    std::uint32_t maxFullSnapshotBytes = 4 * 1024 * 1024;  // 4 MiB

    /// Maximum allowed delta snapshot payload in bytes.
    std::uint32_t maxDeltaSnapshotBytes = 512 * 1024;  // 512 KiB

    /// Compression ratio floor.  If the ratio of decoded entity count
    /// to payload bytes falls below this threshold, the snapshot is
    /// flagged as suspicious (low entropy suggests padding or bloat).
    float minCompressionRatio = 0.01f;  // entities per byte

    /// Maximum number of entities in a single snapshot.
    std::uint32_t maxEntityCount = 131072;
};

// ─── Size check result ─────────────────────────────────────────────────────

enum class SizeCheckResult : std::uint8_t
{
    Ok,
    ExceedsMaxSize,
    EntityCountExceeded,
    SuspiciousCompressionRatio,
};

// ─── Snapshot size guard ───────────────────────────────────────────────────

/// Validates snapshot sizes before they reach the decoder.
class SnapshotSizeGuard
{
public:
    SnapshotSizeGuard() = default;

    void Configure(SnapshotSizeConfig config) noexcept;

    /// Check a full snapshot payload before decoding.
    [[nodiscard]] SizeCheckResult CheckFullSnapshot(
        std::uint32_t payloadBytes,
        std::uint32_t entityCount) const noexcept;

    /// Check a delta snapshot payload before decoding.
    [[nodiscard]] SizeCheckResult CheckDeltaSnapshot(
        std::uint32_t payloadBytes,
        std::uint32_t entityCount) const noexcept;

    /// Record the actual decoded size for compression ratio tracking.
    void RecordDecodedSize(std::uint32_t payloadBytes, std::uint32_t decodedBytes) noexcept;

    /// Return the running average compression ratio.
    [[nodiscard]] float AverageCompressionRatio() const noexcept;

private:
    SnapshotSizeConfig config_{};

    // Running EMA of compression ratio.
    float avgCompressionRatio_ = 0.0f;
    bool  hasRatioSample_ = false;
};

} // namespace SagaEngine::Client::Replication
