/// @file EcsSnapshotApply.h
/// @brief Authority-aware ECS apply functions for replication pipeline.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Concrete `FullSnapshotApplyFn` and `DeltaSnapshotApplyFn`
///          implementations that decode component bytes from snapshot
///          payloads and write them into the client's `WorldState`,
///          respecting the replication authority model (ServerOwned,
///          ClientPredicted, ClientOnly).
///
/// Design rules:
///   - Full snapshots only overwrite ServerOwned + ClientPredicted
///     components.  ClientOnly components are preserved.
///   - Delta snapshots apply patches through a PatchJournal (atomic).
///   - Authority is registered per component type at startup via the
///     `SAGA_REGISTER_COMPONENT_AUTH` macro.

#pragma once

#include "SagaEngine/Client/Replication/SnapshotApplyPipeline.h"
#include "SagaEngine/Client/Replication/PatchJournal.h"

#include <cstdint>
#include <functional>

namespace SagaEngine::Simulation {
class WorldState;
} // namespace SagaEngine::Simulation

namespace SagaEngine::Client::Replication {

// ─── Authority enum ────────────────────────────────────────────────────────

/// Replication authority for a component type.  Determines how the
/// snapshot apply pipeline treats each component during full and
/// delta apply.
enum class ReplicationAuthority : std::uint8_t
{
    ServerOwned,     ///< Server is sole authority; always overwrite.
    ClientPredicted, ///< Client predicts locally; server corrects on delta.
    ClientOnly,      ///< Client-local; replication NEVER touches this.
};

/// Human-readable authority name.
[[nodiscard]] const char* AuthorityToString(ReplicationAuthority auth) noexcept;

// ─── Authority table ───────────────────────────────────────────────────────

/// Flat array mapping component type ID to replication authority.
///
/// Uses a direct-index array (not a hash map) for O(1) lookups with
/// zero cache miss overhead during the hot apply path.  The table
/// size covers all possible component type IDs.
class AuthorityTable
{
public:
    AuthorityTable() = default;

    /// Maximum component type ID the table supports.
    static constexpr std::uint32_t kMaxComponentTypes = 1024;

    /// Register authority for a component type.
    void Register(ECS::ComponentTypeId typeId, ReplicationAuthority auth) noexcept;

    /// Look up authority for a component type.  Returns ClientOnly
    /// for unregistered types (safe default: replication won't touch).
    [[nodiscard]] ReplicationAuthority Get(ECS::ComponentTypeId typeId) const noexcept;

    /// Reset all entries to ClientOnly.
    void Reset() noexcept;

private:
    std::uint8_t table_[kMaxComponentTypes]{};
};

// ─── Registration macro ────────────────────────────────────────────────────

/// Register a component type with a stable ID and replication authority.
///
/// Place calls in a dedicated ComponentRegistration.cpp, executed before
/// any WorldState is constructed.
///
/// Example:
///   SAGA_REGISTER_COMPONENT_AUTH(TransformComponent, 1, ServerOwned);
///   SAGA_REGISTER_COMPONENT_AUTH(InputStateComponent, 5, ClientPredicted);
///   SAGA_REGISTER_COMPONENT_AUTH(CameraComponent, 10, ClientOnly);
#define SAGA_REGISTER_COMPONENT_AUTH(Type, Id, Auth)         \
    ::SagaEngine::Client::Replication::g_AuthorityTable.Register(  \
        (Id),                                                    \
        ::SagaEngine::Client::Replication::ReplicationAuthority::Auth)

/// Global authority table singleton.
extern AuthorityTable g_AuthorityTable;

// ─── Apply function factories ──────────────────────────────────────────────

/// Build a full snapshot apply function that respects authority.
///
/// The returned function:
///   1. Deserializes the server WorldState blob.
///   2. For each server component:
///      - ServerOwned     → overwrite client component
///      - ClientPredicted → reconcile (server wins, update client)
///      - ClientOnly      → SKIP (preserve client-local state)
///   3. Entities not in server state but with ClientOnly components
///      are preserved (not destroyed).
[[nodiscard]] FullSnapshotApplyFn MakeFullSnapshotApplyFn(
    const AuthorityTable& authority);

/// Build a delta snapshot apply function that respects authority.
///
/// The returned function:
///   1. Decodes the delta payload into a PatchJournal.
///   2. Sorts patches deterministically.
///   3. Validates ALL patches (pure, no side effects).
///   4. Commits ALL patches if validation passes; discards all if not.
[[nodiscard]] DeltaSnapshotApplyFn MakeDeltaSnapshotApplyFn(
    const AuthorityTable& authority);

// ─── World hash ────────────────────────────────────────────────────────────

/// Compute a canonical world hash for desync detection.
///
/// Hash includes only ServerOwned + ClientPredicted components.
/// ClientOnly components are excluded to prevent false desync flags.
///
/// Determinism guarantees:
///   - Entities sorted by EntityId ascending
///   - Components sorted by ComponentTypeId ascending
///   - Raw bit pattern for floats (IEEE 754 LE, no conversion)
///   - Zero-padding bytes are explicit zeros in component structs
///   - Endianness: little-endian (x86/ARM LE native)
///
/// @returns 64-bit FNV-1a hash of the canonical world state.
[[nodiscard]] std::uint64_t ComputeCanonicalWorldHash(
    const Simulation::WorldState& world,
    const AuthorityTable& authority) noexcept;

} // namespace SagaEngine::Client::Replication
