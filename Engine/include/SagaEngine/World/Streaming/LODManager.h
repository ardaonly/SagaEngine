/// @file LODManager.h
/// @brief World-level level-of-detail selector driven by distance and budget pressure.
///
/// Layer  : SagaEngine / World / Streaming
/// Purpose: The renderer does not decide on its own which LOD to draw
///          for a given mesh — it asks the `LODManager`.  The manager
///          combines three inputs to produce a per-entity LOD index:
///
///            1. Squared distance from the active camera to the
///               entity's anchor point.  Squared distance is used
///               throughout so we never touch `sqrt` on the hot path.
///            2. The platform's `StreamingBudget` pressure ratio.
///               Under memory pressure the manager biases every
///               entity towards coarser LODs so the decoded mesh
///               cache can stay under its soft limit.
///            3. Per-entity overrides (critical NPC, quest item,
///               cinematic lock) that pin the entity to a specific
///               LOD regardless of distance or pressure.
///
///          The output is an integer in `[0, kMaxMeshLodCount - 1]`
///          that matches the LOD slot layout in `Render::MeshAsset`.
///          The renderer consumes it directly; no further arithmetic
///          is needed on the draw submission side.
///
/// Design rules:
///   - Squared distances throughout.  Callers pass the squared
///     camera→entity distance; no sqrt on the hot path.
///   - Monotone thresholds.  The LOD thresholds are an increasing
///     array so a binary search is unnecessary — a linear scan is
///     fast enough for four LODs and keeps the code cache warm.
///   - Pressure biases, it does not override.  The budget pressure
///     adds a configurable offset to the LOD index; a pinned entity
///     stays pinned even under extreme pressure.
///   - No allocation on the hot path.  The manager is a pure value
///     computation: given the inputs, it returns an integer with
///     zero heap touches.
///
/// What this header is NOT:
///   - Not a culling primitive.  Whether an entity is drawn at all
///     is a frustum-culling decision made by the render graph;
///     this header only decides which mip / LOD to bind.
///   - Not a streaming orchestrator.  Which LODs are resident is
///     the `ResourceStream`'s job; this manager just consumes the
///     residency map and reports the best available LOD.

#pragma once

#include "SagaEngine/Resources/StreamingBudget.h"

#include <array>
#include <cstdint>

namespace SagaEngine::World::Streaming {

// ─── Tuning constants ──────────────────────────────────────────────────────

/// Maximum number of LOD slots the manager understands.  Must stay
/// in lock-step with the fixed LOD-array size in `Render::MeshAsset`;
/// both must agree to within a static assert so a mismatch fails the
/// build rather than the frame.
inline constexpr std::uint8_t kMaxMeshLods = 4;

/// Largest LOD index the manager can emit (inclusive).  Equivalent
/// to `kMaxMeshLods - 1`; aliased here so consumers that only care
/// about the selector output do not need to touch the array size.
inline constexpr std::uint8_t kMaxLodIndex = kMaxMeshLods - 1;

static_assert(kMaxMeshLods >= 1,
              "LODManager requires at least one mesh LOD slot");

// ─── Configuration ─────────────────────────────────────────────────────────

/// Per-platform tuning for the LOD manager.  Defaults are biased
/// towards a desktop profile; mobile profiles bump the thresholds
/// inward so coarse LODs kick in sooner.
struct LodManagerConfig
{
    /// Squared-distance thresholds in world-space metres squared.
    /// Entity at distance² < `distanceSqThresholds[i]` uses LOD `i`.
    /// The last slot in the array is the upper bound for the coarsest
    /// LOD — anything beyond it is still drawn at the coarsest LOD
    /// until the culler removes it.
    ///
    /// The defaults land at 25 m, 100 m, 300 m, 1000 m for LODs 0..3.
    std::array<float, kMaxMeshLods> distanceSqThresholds {
        25.0f    * 25.0f,     // LOD 0 inside 25 m.
        100.0f   * 100.0f,    // LOD 1 inside 100 m.
        300.0f   * 300.0f,    // LOD 2 inside 300 m.
        1000.0f  * 1000.0f,   // LOD 3 inside 1 km.
    };

    /// Pressure-driven LOD bias.  Under maximum streaming pressure
    /// (ratio == 1.0) every entity is shifted this many LOD levels
    /// coarser than its distance-based answer.  A bias of `2` under
    /// full pressure moves a character from LOD 0 to LOD 2, which
    /// typically halves the decoded vertex count.
    std::uint8_t maxPressureBias = 2;

    /// Pressure ratio below which no bias is applied.  Keeping this
    /// above zero avoids boundary flicker in steady-state scenes
    /// where pressure hovers around 0.1–0.2.
    float pressureBiasDeadZone = 0.25f;
};

// ─── Per-entity LOD query input ────────────────────────────────────────────

/// Everything the manager needs to emit a single LOD index.  Passing
/// a struct instead of a long parameter list keeps the signature
/// stable as the feature set grows.
struct LodQuery
{
    /// Squared distance from the camera to the entity in metres².
    float distanceSq = 0.0f;

    /// Optional pin to a specific LOD; values above `kMaxLodIndex`
    /// are treated as "no pin".  Cinematic cameras and highlight
    /// characters set this to 0 so they always draw at the best LOD.
    std::uint8_t pinnedLod = 0xFFu;

    /// Coarsest LOD actually resident for this mesh, as reported by
    /// the streaming subsystem.  The manager never returns a LOD
    /// finer than what is resident — there is no point asking the
    /// renderer to draw with data that is not in memory.
    std::uint8_t residencyFloor = 0;
};

// ─── LODManager ────────────────────────────────────────────────────────────

/// Stateless LOD selector.  Holds only the configuration; all inputs
/// come in through `Select`.  The manager is safe to call from any
/// thread and produces deterministic output for identical input.
class LODManager
{
public:
    LODManager() noexcept  = default;
    ~LODManager() noexcept = default;

    // ─── Configuration ───────────────────────────────────────────────────

    /// Replace the configuration.  Intended for boot-time setup and
    /// for the in-editor "simulate low-end hardware" toggle.
    void Configure(LodManagerConfig config) noexcept { config_ = config; }

    /// Borrow the current configuration for diagnostics panels.
    [[nodiscard]] const LodManagerConfig& Config() const noexcept { return config_; }

    // ─── Selection ───────────────────────────────────────────────────────

    /// Compute the LOD index for a single entity.  `pressureRatio`
    /// is the current streaming-budget pressure reported by
    /// `Resources::StreamingBudget::Pressure` — a value in [0, 1]
    /// where 0 means "well under the soft limit" and 1 means "at
    /// the hard limit".
    [[nodiscard]] std::uint8_t Select(const LodQuery& query,
                                      float           pressureRatio) const noexcept;

    /// Convenience overload that queries the budget directly so
    /// gameplay code does not have to pass the ratio in.  The budget
    /// reference must outlive the call.
    [[nodiscard]] std::uint8_t Select(
        const LodQuery&                               query,
        const SagaEngine::Resources::StreamingBudget& budget) const noexcept;

private:
    LodManagerConfig config_;
};

} // namespace SagaEngine::World::Streaming
