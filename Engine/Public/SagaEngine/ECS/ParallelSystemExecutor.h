/// @file ParallelSystemExecutor.h
/// @brief Runs ECS systems in parallel under the TaskGraph scheduler.
///
/// Layer  : SagaEngine / ECS
/// Purpose: By default a `WorldState` calls `OnSystemUpdate` on its
///          systems sequentially, in registration order.  That is the
///          right behaviour for deterministic replays and for any
///          system that reads shared data, but it leaves a lot of
///          compute on the floor: on an 8-core box, four independent
///          systems still run one after the other on a single thread.
///
///          `ParallelSystemExecutor` is the opt-in alternative.  The
///          caller declares dependency edges between systems (system A
///          reads what system B writes, so B must finish before A
///          starts); the executor builds a task graph from those edges
///          and runs every independent chain in parallel via the
///          engine `TaskGraph`, which in turn rides on `JobSystem`.
///
/// Design rules:
///   - Dependencies are *declared*, not inferred.  Inference would
///     require either component-access annotations on every system
///     (a huge refactor) or conservative "everything depends on
///     everything" (which defeats the point).  Declarative is explicit
///     and reviewable.
///   - The executor is a strictly additive layer on top of `ISystem`.
///     Existing systems don't need to know they're being run in
///     parallel; the executor calls the same `OnSystemUpdate(dt)` entry
///     point.  Systems that are NOT thread-safe simply must not be
///     declared parallel — there is no runtime enforcement.
///   - Registration order does NOT imply a dependency.  Two systems
///     with no declared edge between them MAY run in parallel; a caller
///     who relied on registration order for sequencing is a caller
///     with a latent bug the executor will expose.
///
/// Threading:
///   `Execute()` is called on the simulation thread.  It blocks until
///   the wave-by-wave schedule completes, so from the caller's point of
///   view the semantics are identical to calling each system in turn.
///
/// Related headers:
///   - `TaskGraph.h`  (Kahn + wave scheduler used under the hood)
///   - `System.h`     (ISystem contract)
///   - `JobSystem.h`  (worker pool doing the actual parallelism)

#pragma once

#include "SagaEngine/Core/Threading/TaskGraph.h"
#include "SagaEngine/ECS/System.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEngine::ECS {

// ─── Handle ─────────────────────────────────────────────────────────────────

/// Opaque handle returned by `AddSystem`.  Used to declare dependency
/// edges — keeping the handle opaque means callers cannot accidentally
/// read a pointer out of it and hold a dangling reference past Reset().
enum class ParallelSystemHandle : std::uint16_t { kInvalid = 0xFFFFu };

// ─── Executor ───────────────────────────────────────────────────────────────

/// Runs a configured set of `ISystem` instances in parallel where the
/// declared dependency graph allows.  One instance per `WorldState`
/// that opts in to parallel ticks; construct once and reuse across
/// frames so the underlying `TaskGraph` keeps its allocated capacity.
class ParallelSystemExecutor
{
public:
    /// Construct with an already-configured `TaskGraph`.  Taking the
    /// graph by reference (not owning it) lets several executors share
    /// one graph instance across systems that get ticked at different
    /// rates — though most callers will construct a private graph.
    explicit ParallelSystemExecutor(SagaEngine::Core::TaskGraph& graph) noexcept
        : graph_(graph)
    {
    }

    // ── Configuration ─────────────────────────────────────────────────────

    /// Register a system under `debugName`.  `system` must outlive the
    /// executor — the executor does NOT take ownership, matching the
    /// ECS convention that systems are managed by the `WorldState`.
    /// Returns `kInvalid` if the underlying graph is at capacity.
    [[nodiscard]] ParallelSystemHandle AddSystem(
        std::string debugName,
        ISystem*    system);

    /// Declare "successor must run after predecessor finishes in the
    /// current frame".  Adding the reverse of an existing edge yields a
    /// cycle which `Execute()` will refuse to run.  Self-dependencies
    /// are silently ignored — they are never useful and usually come
    /// from a copy-paste bug.
    void AddDependency(
        ParallelSystemHandle predecessor,
        ParallelSystemHandle successor);

    // ── Execution ─────────────────────────────────────────────────────────

    /// Tick every registered system for this frame.  Returns `true` on
    /// a clean run and `false` if the dependency graph contained a
    /// cycle — in which case `TaskGraph::Execute` has already logged
    /// the offending nodes.  The executor makes no attempt at partial
    /// recovery; a cycle is a programming error and fixing it is the
    /// only answer.
    [[nodiscard]] bool Execute(float dt);

    /// Clear all registered systems and edges while keeping the
    /// capacity of the underlying graph.  Use between level loads
    /// where the system roster changes but the executor instance
    /// itself is preserved.
    void Reset();

    // ── Diagnostics ───────────────────────────────────────────────────────

    [[nodiscard]] std::size_t SystemCount() const noexcept { return entries_.size(); }

private:
    struct Entry
    {
        std::string                            debugName;
        ISystem*                               system = nullptr;
        SagaEngine::Core::TaskGraph::NodeHandle node =
            SagaEngine::Core::TaskGraph::NodeHandle::kInvalid;
    };

    SagaEngine::Core::TaskGraph& graph_;
    std::vector<Entry>           entries_;

    /// Per-frame delta time published by `Execute()`.  Lives on the
    /// executor so the per-node closures registered by `AddSystem` can
    /// pick up the value for the current frame without rebuilding the
    /// task graph on every tick.
    float latestDt_ = 0.0f;
};

} // namespace SagaEngine::ECS
