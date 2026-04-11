/// @file PatchJournal.h
/// @brief Transactional patch journal for atomic ECS apply.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Decodes wire-format delta/full snapshot data into a sorted
///          list of patches, validates every patch in a side-effect-free
///          pass, and commits only if all validations succeed.  This is
///          the guard against partial-state corruption: if any patch
///          fails validation, the entire journal is discarded.
///
/// Design rules:
///   - Deterministic ordering: patches sorted by (entityId, generation,
///     componentTypeId) so apply order is identical across all clients.
///   - Two-phase apply: Validate (pure, no side effects) → Commit
///     (deterministic, applies to WorldState).
///   - Memory bounded: max 4096 patches per journal, max 64 KiB byte
///     budget, pre-allocated storage (no heap during commit).
///   - Time budget guard: if apply exceeds maxApplyTimePerFrame, the
///     journal yields to the next frame (tick-level, not patch-level).

#pragma once

#include "SagaEngine/ECS/Component.h"
#include "SagaEngine/ECS/Entity.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace SagaEngine::Simulation {
class WorldState;
} // namespace SagaEngine::Simulation

namespace SagaEngine::Client::Replication {

// ─── Patch types ───────────────────────────────────────────────────────────

/// Maximum component data inlined into a spawn patch.  Larger blobs
/// are stored out-of-line and referenced via pointer.
inline constexpr std::size_t kMaxInlineComponentData = 24;

/// Maximum components per entity in a spawn record.
inline constexpr std::size_t kMaxSpawnComponents = 32;

/// Entity spawn patch.  Carries component data for a new entity.
/// Uses aggregate initialization — all members zero-initialized.
struct EntitySpawnPatch
{
    std::uint32_t entityId;
    std::uint32_t generation;
    std::uint8_t  componentCount;

    struct ComponentEntry
    {
        ECS::ComponentTypeId typeId;
        std::uint16_t        dataLen;
        std::uint8_t         inlineData[kMaxInlineComponentData];
        const std::uint8_t*  dataPtr;
    } components[kMaxSpawnComponents];
};

/// Component write patch.  Non-owning data reference.
struct ComponentWritePatch
{
    std::uint32_t              entityId;
    ECS::ComponentTypeId       typeId;
    const std::uint8_t*        data;
    std::uint16_t              dataLen;
};

/// Component removal patch.
struct ComponentRemovePatch
{
    std::uint32_t        entityId;
    ECS::ComponentTypeId typeId;
};

/// Entity despawn patch.  Generation must match current entity gen.
struct EntityDespawnPatch
{
    std::uint32_t entityId;
    std::uint32_t generation;
};

// ─── Discriminated union ───────────────────────────────────────────────────

/// Tag identifying which patch variant is active.
enum class PatchType : std::uint8_t
{
    Spawn,
    ComponentWrite,
    ComponentRemove,
    Despawn,
};

/// A single patch entry in the journal.
struct Patch
{
    PatchType type = PatchType::ComponentWrite;
    union
    {
        EntitySpawnPatch      spawn;
        ComponentWritePatch   write;
        ComponentRemovePatch  remove;
        EntityDespawnPatch    despawn;
    } data;

    Patch() noexcept { std::memset(&data, 0, sizeof(data)); }
    ~Patch() noexcept = default;

    Patch(const Patch& other) noexcept;
    Patch& operator=(const Patch& other) noexcept;
    Patch(Patch&& other) noexcept;
    Patch& operator=(Patch&& other) noexcept;
};

// ─── Validation result ─────────────────────────────────────────────────────

enum class ValidateResult : std::uint8_t
{
    Ok,
    EntityNotFound,
    EntityAlreadyExists,
    ComponentNotRegistered,
    ComponentNotFound,
    GenerationMismatch,
    DataSizeMismatch,
};

// ─── Patch journal ─────────────────────────────────────────────────────────

/// Bounded, transactional patch journal.
///
/// Build patches from decoded wire data → Sort deterministically →
/// Validate all → Commit all.  If any step fails, discard the entire
/// journal — no partial writes reach the ECS.
///
/// Thread-affine: all methods must be called from the game thread.
class PatchJournal
{
public:
    PatchJournal() = default;
    ~PatchJournal() = default;

    PatchJournal(const PatchJournal&)            = delete;
    PatchJournal& operator=(const PatchJournal&) = delete;

    /// Maximum patches per journal (compile-time bound).
    static constexpr std::size_t kMaxPatches = 4096;

    /// Maximum total byte budget for a journal.
    static constexpr std::size_t kMaxByteBudget = 64 * 1024;

    // ─── Building ────────────────────────────────────────────────────────

    /// Add a spawn patch for a new entity.
    bool PushSpawn(EntitySpawnPatch patch) noexcept;

    /// Add a component write patch.
    bool PushWrite(ComponentWritePatch patch) noexcept;

    /// Add a component removal patch.
    bool PushRemove(ComponentRemovePatch patch) noexcept;

    /// Add a despawn patch for an entity.
    bool PushDespawn(EntityDespawnPatch patch) noexcept;

    /// Return true if adding another patch would exceed bounds.
    [[nodiscard]] bool IsFull() const noexcept;

    /// Return the total estimated byte footprint of this journal.
    [[nodiscard]] std::size_t EstimatedBytes() const noexcept;

    /// Clear all patches and reset counters.
    void Clear() noexcept;

    // ─── Deterministic ordering ──────────────────────────────────────────

    /// Sort patches by (entityId ASC, generation ASC, componentTypeId ASC).
    /// Must be called before Validate and Commit.
    void Sort() noexcept;

    // ─── Two-phase apply ─────────────────────────────────────────────────

    /// Phase 1: Validate every patch (pure, no side effects).
    /// Returns Ok if all patches are valid, or the first failure reason.
    [[nodiscard]] ValidateResult Validate(Simulation::WorldState& world) const noexcept;

    /// Phase 2: Commit every patch to the WorldState (deterministic order).
    /// Returns Ok if all commits succeed.  Must be called after Validate.
    [[nodiscard]] ValidateResult Commit(Simulation::WorldState& world) const noexcept;

    /// Full atomic apply: Sort → Validate → Commit.
    /// Returns Ok only if all three steps succeed.
    [[nodiscard]] ValidateResult Apply(Simulation::WorldState& world) noexcept;

    [[nodiscard]] std::size_t PatchCount() const noexcept { return patchCount_; }

private:
    std::array<Patch, kMaxPatches> patches_{};
    std::size_t                    patchCount_ = 0;
};

} // namespace SagaEngine::Client::Replication
