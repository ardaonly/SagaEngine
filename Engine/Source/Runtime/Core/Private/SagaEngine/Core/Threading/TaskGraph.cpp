/// @file TaskGraph.cpp
/// @brief Implementation of the dependency-aware TaskGraph scheduler.

#include "SagaEngine/Core/Threading/TaskGraph.h"

#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <queue>

namespace SagaEngine::Core {

// ─── AddNode ────────────────────────────────────────────────────────────────

TaskGraph::NodeHandle TaskGraph::AddNode(std::string debugName, std::function<void()> work)
{
    // Hard cap guards against runaway graphs caused by accidentally
    // submitting nodes inside a loop.  Returning kInvalid is preferable
    // to throwing because engine code compiles without exceptions.
    if (nodes_.size() >= kMaxNodes)
    {
        LOG_ERROR("Threading", "TaskGraph: exceeded kMaxNodes (%u)",
                  static_cast<unsigned>(kMaxNodes));
        return NodeHandle::kInvalid;
    }

    Node n;
    n.debugName = std::move(debugName);
    n.work      = std::move(work);
    // successors stays empty until AddDependency fills it in.
    nodes_.push_back(std::move(n));

    return static_cast<NodeHandle>(nodes_.size() - 1);
}

// ─── AddDependency ──────────────────────────────────────────────────────────

bool TaskGraph::AddDependency(NodeHandle predecessor, NodeHandle successor)
{
    const auto pi = static_cast<std::uint16_t>(predecessor);
    const auto si = static_cast<std::uint16_t>(successor);

    if (pi == static_cast<std::uint16_t>(NodeHandle::kInvalid) ||
        si == static_cast<std::uint16_t>(NodeHandle::kInvalid) ||
        pi >= nodes_.size() || si >= nodes_.size() ||
        pi == si)
    {
        // Self-dependency is always a bug (instant cycle).  Bogus indices
        // are rejected rather than silently extending the vector.
        return false;
    }

    auto& predSuccessors = nodes_[pi].successors;

    // Fold duplicates — calling AddDependency(A, B) twice must not bump
    // B's predecessor counter twice, otherwise it would never unblock.
    if (std::find(predSuccessors.begin(), predSuccessors.end(), si) == predSuccessors.end())
    {
        predSuccessors.push_back(si);
        nodes_[si].predecessorCount += 1;
    }

    return true;
}

// ─── TopologicalOrder ───────────────────────────────────────────────────────

std::vector<std::uint16_t> TaskGraph::TopologicalOrder() const
{
    // Kahn's algorithm: start with every node that has zero predecessors,
    // walk the graph draining counters, and collect nodes in the order
    // they unlock.  If fewer than N nodes were visited at the end, there
    // must be a cycle (unreachable nodes).
    std::vector<std::uint16_t> order;
    order.reserve(nodes_.size());

    std::vector<std::uint16_t> remaining(nodes_.size(), 0);
    for (std::size_t i = 0; i < nodes_.size(); ++i)
        remaining[i] = nodes_[i].predecessorCount;

    std::queue<std::uint16_t> ready;
    for (std::size_t i = 0; i < nodes_.size(); ++i)
    {
        if (remaining[i] == 0)
            ready.push(static_cast<std::uint16_t>(i));
    }

    while (!ready.empty())
    {
        const std::uint16_t u = ready.front();
        ready.pop();
        order.push_back(u);

        for (const std::uint16_t v : nodes_[u].successors)
        {
            if (--remaining[v] == 0)
                ready.push(v);
        }
    }

    if (order.size() != nodes_.size())
    {
        // Cycle detected — log every node that still has a non-zero
        // predecessor count so the user can untangle it.
        for (std::size_t i = 0; i < nodes_.size(); ++i)
        {
            if (remaining[i] != 0)
            {
                LOG_ERROR("Threading",
                          "TaskGraph: cycle detected at node '%s'",
                          nodes_[i].debugName.c_str());
            }
        }
        return {}; // Signal failure.
    }

    return order;
}

// ─── Execute ────────────────────────────────────────────────────────────────

bool TaskGraph::Execute()
{
    if (jobSystem_ == nullptr)
    {
        LOG_ERROR("Threading", "TaskGraph: no JobSystem bound");
        return false;
    }
    if (nodes_.empty())
        return true;

    const std::vector<std::uint16_t> order = TopologicalOrder();
    if (order.empty())
        return false; // Cycle — already logged.

    // Wave scheduler: group topologically ordered nodes into waves where
    // every node in a wave has all its predecessors in earlier waves.
    // We compute each node's "depth" (longest distance from a root) and
    // bucket by depth.  This is O(V + E).
    std::vector<std::uint16_t> depth(nodes_.size(), 0);
    std::uint16_t              maxDepth = 0;
    for (const std::uint16_t u : order)
    {
        const std::uint16_t d = depth[u];
        if (d > maxDepth) maxDepth = d;
        for (const std::uint16_t v : nodes_[u].successors)
        {
            if (depth[v] < d + 1)
                depth[v] = static_cast<std::uint16_t>(d + 1);
        }
    }

    std::vector<std::vector<std::uint16_t>> waves(maxDepth + 1);
    for (std::size_t i = 0; i < nodes_.size(); ++i)
        waves[depth[i]].push_back(static_cast<std::uint16_t>(i));

    // Run waves sequentially, barriering between them.  Inside a wave,
    // every node is independent — fan them out to the JobSystem and
    // collect handles so we can Wait() before the next wave starts.
    for (const auto& wave : waves)
    {
        std::vector<JobHandle> handles;
        handles.reserve(wave.size());
        for (const std::uint16_t idx : wave)
        {
            // Capture by reference is safe: `nodes_` outlives Execute().
            auto& workRef = nodes_[idx].work;
            handles.push_back(jobSystem_->Schedule([&workRef]() {
                if (workRef) workRef();
            }));
        }
        for (const auto& h : handles)
            h.Wait();
    }

    return true;
}

// ─── Reset ──────────────────────────────────────────────────────────────────

void TaskGraph::Reset() noexcept
{
    // Clear keeps the outer vector's capacity.  Per-node successor
    // vectors are thrown away with the Node, which is fine — they get
    // re-allocated next frame anyway, and shrink-to-fit on every reset
    // would be counterproductive.
    nodes_.clear();
}

} // namespace SagaEngine::Core
