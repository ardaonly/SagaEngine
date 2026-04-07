/// @file Deterministic.h
/// @brief Tick-by-tick hash tracking for server/client divergence detection.
///
/// Every authoritative simulation tick produces a WorldState hash. The server
/// piggybacks recent hashes onto replication packets. The client compares
/// received hashes against its own predicted state at the same tick.
///
/// A hash mismatch at tick T means the simulation diverged at or before T.
/// The client then discards its predicted state and reconciles from the
/// server's authoritative snapshot (ClientPrediction::Reconcile).
///
/// Architecture note:
///   Deterministic does NOT access the network layer. It only tracks hashes
///   and exposes them for ReplicationManager to embed in outgoing packets.
///   This keeps the determinism subsystem independent of transport concerns.

#pragma once

#include "SagaEngine/Simulation/WorldState.h"

#include <array>
#include <cstdint>
#include <optional>

namespace SagaEngine::Simulation {

/// Rolling tick-hash history with divergence reporting.
class Deterministic
{
public:
    /// Depth of the hash ring buffer.
    ///
    /// At 64 Hz, 256 ticks = 4 seconds of history. Server and client
    /// must reconcile within this window; beyond it the connection is dropped.
    static constexpr uint32_t kHistoryDepth = 256u;

    // ─── Recording ─────────────────────────────────────────────────────────────

    /// Compute and record the hash for the given WorldState at tickNumber.
    ///
    /// Call this at the end of each simulation tick, after all systems have run.
    /// @returns The computed hash (also stored internally).
    uint64_t Record(uint64_t tickNumber, const WorldState& state) noexcept;

    /// Store an externally computed hash (e.g. received from client for logging).
    void RecordExternal(uint64_t tickNumber, uint64_t hash) noexcept;

    // ─── Lookup ────────────────────────────────────────────────────────────────

    /// Return the stored hash at tickNumber, or nullopt if outside the window.
    [[nodiscard]] std::optional<uint64_t> HashAt(uint64_t tickNumber) const noexcept;

    /// Return the most recently recorded tick number.
    [[nodiscard]] uint64_t LatestTick() const noexcept { return m_latestTick; }

    // ─── Verification ──────────────────────────────────────────────────────────

    /// Compare a remote hash against the locally recorded hash for the same tick.
    ///
    /// @returns true if hashes match (or if tick is outside the history window).
    ///          false signals a confirmed divergence at tickNumber.
    [[nodiscard]] bool Verify(uint64_t tickNumber, uint64_t remoteHash) const noexcept;

    // ─── Reset ─────────────────────────────────────────────────────────────────

    /// Clear all history. Call on reconnect or level reload.
    void Reset() noexcept;

private:
    struct Entry
    {
        uint64_t tick{0};     ///< Simulation tick this hash belongs to.
        uint64_t hash{0};     ///< FNV-1a hash of the WorldState at that tick.
        bool     valid{false};///< False when the slot has never been written.
    };

    std::array<Entry, kHistoryDepth> m_ring{};   ///< Ring buffer of tick hashes.
    uint64_t                         m_latestTick{0};

    /// Map tick number to ring buffer slot index.
    [[nodiscard]] static uint32_t SlotFor(uint64_t tick) noexcept
    {
        return static_cast<uint32_t>(tick % kHistoryDepth);
    }
};

} // namespace SagaEngine::Simulation
