/// @file TaskGraph.h
/// @brief Dependency-aware task scheduler built on top of the JobSystem.
///
/// Layer  : SagaEngine / Core / Threading
/// Purpose: The JobSystem dispatches independent work.  That's not enough
///          for frame execution: physics must finish before movement
///          replication, movement replication must finish before the
///          network flush, etc.  A TaskGraph wraps JobSystem with an
///          explicit DAG so callers describe "A before B" once and the
///          scheduler figures out when every node can run.
///
/// Design:
///   - Nodes are submitted up-front; edges are added with `AddDependency`.
///     Cycles are detected at `Execute()` time via topological sort.
///   - Execution fans nodes out to the JobSystem in "waves": every node
///     whose dependencies have finished is scheduled in parallel, then we
///     barrier and pick up the next wave.  Not the most throughput-optimal
///     model (Kahn-style with fine-grained predecessor counters fanning
///     out lazily would be better) but it's correct, lock-free on the
///     critical path, and trivially debuggable.
///   - Each TaskGraph is reusable — call `Reset()` between frames to
///     clear nodes but keep the allocation.
///   - Node capacity is capped at 4096 per graph; a top-level frame
///     pipeline rarely exceeds a few hundred.
///
/// Why not "just use TBB flow graph":
///   oneTBB is a later-milestone dependency; we need dependency scheduling
///   *now* for the frame pipeline.  This implementation is ~200 lines and
///   uses only the stdlib plus the existing JobSystem, so it ports to
///   every target and stays debuggable under a stack walker.

#pragma once

#include "SagaEngine/Core/Threading/JobSystem.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace SagaEngine::Core {

// ─── TaskGraph ──────────────────────────────────────────────────────────────

/// Lightweight DAG scheduler.  One instance per frame / per logical graph.
/// Not thread-safe for building — construction must happen on a single
/// thread, typically the main thread.  Execution uses the shared JobSystem.
class TaskGraph
{
public:
    /// Opaque node handle.  A typed handle rather than a raw index catches
    /// accidental arithmetic mistakes at compile time and keeps invalid
    /// handles distinguishable from valid ones.
    enum class NodeHandle : std::uint16_t
    {
        kInvalid = 0xFFFFu
    };

    /// Build-time hard cap.  Bounding the node count keeps predecessor /
    /// successor vectors contiguous and prevents runaway graphs.
    static constexpr std::uint16_t kMaxNodes = 4096;

    explicit TaskGraph(JobSystem& jobSystem) noexcept
        : jobSystem_(&jobSystem)
    {
        nodes_.reserve(128);
    }

    TaskGraph(const TaskGraph&)            = delete;
    TaskGraph& operator=(const TaskGraph&) = delete;
    TaskGraph(TaskGraph&&)                 = delete;
    TaskGraph& operator=(TaskGraph&&)      = delete;

    // ── Graph construction ────────────────────────────────────────────────

    /// Add a node that runs `work` with no initial dependencies.  `debugName`
    /// is captured for logs / profiler output only — pass a stable string
    /// literal whenever possible.  Returns `NodeHandle::kInvalid` if the
    /// graph is already at `kMaxNodes`.
    [[nodiscard]] NodeHandle AddNode(std::string debugName, std::function<void()> work);

    /// Declare "predecessor must finish before successor starts".  Safe to
    /// call multiple times with the same pair (duplicates are folded into
    /// the successor list).  Returns false on invalid handles; the graph
    /// is left unchanged in that case.
    bool AddDependency(NodeHandle predecessor, NodeHandle successor);

    // ── Execution ─────────────────────────────────────────────────────────

    /// Validate the graph and run it to completion.  Blocks the caller
    /// until every node has finished.  Returns false if the graph contains
    /// a cycle — the offending nodes are logged with their debug names and
    /// no node is executed.
    bool Execute();

    /// Discard all nodes and edges while keeping the underlying vectors'
    /// capacity.  The JobSystem pointer is preserved across resets.
    void Reset() noexcept;

    /// Diagnostic accessors.
    [[nodiscard]] std::size_t NodeCount() const noexcept { return nodes_.size(); }

private:
    /// Per-node bookkeeping.  A struct-of-arrays layout would be faster
    /// but harder to debug; at the 4096-node cap the memory cost is tiny.
    struct Node
    {
        std::string                debugName;
        std::function<void()>      work;
        std::vector<std::uint16_t> successors;    ///< Indices of nodes we unblock.
        std::uint16_t              predecessorCount = 0;
    };

    JobSystem*        jobSystem_ = nullptr;
    std::vector<Node> nodes_;

    /// Build a topological run order.  Returns an empty vector if the
    /// graph contains a cycle.  O(V + E) via Kahn's algorithm.
    [[nodiscard]] std::vector<std::uint16_t> TopologicalOrder() const;
};

} // namespace SagaEngine::Core
