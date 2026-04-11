/// @file GranularWorldHash.h
/// @brief Subsystem-level and component-level hash for desync debugging.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: The canonical world hash (FNV-1a 64-bit) detects that a
///          desync occurred, but not where.  This module provides
///          granular hashing at the subsystem and per-entity level
///          so operators can pinpoint the exact divergence source
///          without replaying the entire simulation.
///
/// Design rules:
///   - Subsystem hashes are computed independently (Transform, Physics,
///     Health, etc.) so a mismatch isolates to one domain.
///   - Per-entity hashes are computed on demand for the flagged entity.
///   - Diff reporting identifies the exact byte offset and component
///     type where client and server diverge.

#pragma once

#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/ECS/Component.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Simulation {
class WorldState;
} // namespace SagaEngine::Simulation

namespace SagaEngine::Client::Replication {

class AuthorityTable;

// ─── Subsystem hash entry ──────────────────────────────────────────────────

/// Hash result for one component type across all entities.
struct SubsystemHashEntry
{
    ECS::ComponentTypeId typeId = 0;
    std::uint64_t        hash = 0;
    std::uint32_t        entityCount = 0;
};

// ─── Per-entity component diff ──────────────────────────────────────────────

/// Byte-level diff between client and server for one component.
struct ComponentByteDiff
{
    ECS::ComponentTypeId typeId = 0;
    std::uint32_t        entityId = 0;
    std::uint32_t        byteOffset = 0;
    std::uint8_t         clientByte = 0;
    std::uint8_t         serverByte = 0;
};

// ─── Granular hash result ──────────────────────────────────────────────────

struct GranularHashResult
{
    /// Overall world hash (matches ComputeCanonicalWorldHash).
    std::uint64_t worldHash = 0;

    /// Per-subsystem hashes (one entry per registered component type).
    std::vector<SubsystemHashEntry> subsystemHashes;

    /// True if the client's world hash matches the server's.
    bool matches = true;

    /// If matches is false, the first mismatching subsystem typeId.
    ECS::ComponentTypeId mismatchSubsystem = 0;
};

// ─── Public API ─────────────────────────────────────────────────────────────

/// Compute granular hashes for the entire world state.
///
/// @param world       The client's current WorldState.
/// @param authority   Authority table for filtering ClientOnly components.
/// @returns           Granular hash result with per-subsystem breakdown.
[[nodiscard]] GranularHashResult ComputeGranularHashes(
    const Simulation::WorldState& world,
    const AuthorityTable& authority) noexcept;

/// Compare client and server component bytes for one entity.
///
/// Returns a list of byte-level differences.  Empty list means
/// the entity is identical on both sides.
///
/// @param clientWorld   The client's WorldState.
/// @param serverData    Raw serialised component bytes from the server.
/// @param serverSize    Byte count of serverData.
/// @param entityId      The entity to compare.
/// @returns             List of byte diffs (empty if identical).
[[nodiscard]] std::vector<ComponentByteDiff> DiffEntityComponents(
    const Simulation::WorldState& clientWorld,
    const std::uint8_t* serverData,
    std::size_t serverSize,
    ECS::EntityId entityId) noexcept;

/// Format a granular hash result as a human-readable report.
///
/// Example output:
///   World hash: 0x1234567890ABCDEF (MISMATCH)
///   Subsystem mismatches:
///     TransformComponent (id=1): client=0xAAA..., server=0xBBB...
///     HealthComponent    (id=5): client=0xCCC..., server=0xCCC... (ok)
[[nodiscard]] std::string FormatHashReport(const GranularHashResult& result,
                                           std::uint64_t serverWorldHash) noexcept;

} // namespace SagaEngine::Client::Replication
