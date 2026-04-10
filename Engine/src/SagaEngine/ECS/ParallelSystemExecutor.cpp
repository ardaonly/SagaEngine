/// @file ParallelSystemExecutor.cpp
/// @brief Implementation of the TaskGraph-backed parallel ECS executor.

#include "SagaEngine/ECS/ParallelSystemExecutor.h"

#include <utility>

namespace SagaEngine::ECS {

// ─── AddSystem ──────────────────────────────────────────────────────────────

ParallelSystemHandle ParallelSystemExecutor::AddSystem(
    std::string debugName,
    ISystem*    system)
{
    // Reject null systems at registration time.  A null pointer stored
    // in the table would explode later during Execute(); failing here
    // keeps the blast radius at the misuse site.
    if (system == nullptr)
        return ParallelSystemHandle::kInvalid;

    // Capture by value for the lambda — the executor outlives any single
    // Execute() call, and we want the per-frame `dt` to come from the
    // caller, not a stale snapshot.  `system` is a raw pointer owned by
    // the caller, so capturing it is cheap and safe as long as the
    // caller honours the "outlive the executor" contract.
    const auto node = graph_.AddNode(
        debugName,
        [system, currentDt = &latestDt_]() {
            system->OnSystemUpdate(*currentDt);
        });

    if (node == SagaEngine::Core::TaskGraph::NodeHandle::kInvalid)
        return ParallelSystemHandle::kInvalid;

    // Handle is just the index into `entries_`.  Anything smaller
    // than the cap fits in 16 bits — we reject above that in the
    // narrowing check so the static_cast is always safe.
    const std::size_t index = entries_.size();
    if (index >= 0xFFFEu)
        return ParallelSystemHandle::kInvalid;

    entries_.push_back(Entry{ std::move(debugName), system, node });
    return static_cast<ParallelSystemHandle>(index);
}

// ─── AddDependency ──────────────────────────────────────────────────────────

void ParallelSystemExecutor::AddDependency(
    ParallelSystemHandle predecessor,
    ParallelSystemHandle successor)
{
    // Silently drop self-dependencies.  They never mean anything useful
    // and almost always originate from a copy-paste bug — logging would
    // just spam the console.
    if (predecessor == successor)
        return;

    const auto predIdx = static_cast<std::size_t>(predecessor);
    const auto succIdx = static_cast<std::size_t>(successor);

    if (predIdx >= entries_.size() || succIdx >= entries_.size())
        return;

    graph_.AddDependency(entries_[predIdx].node, entries_[succIdx].node);
}

// ─── Execute ────────────────────────────────────────────────────────────────

bool ParallelSystemExecutor::Execute(float dt)
{
    // Stash dt in a member so the per-node closures registered in
    // AddSystem see the value for *this* frame without paying the cost
    // of rebuilding the task graph every tick.  The member lives in the
    // executor, so lifetime is as long as the closures need it.
    latestDt_ = dt;

    return graph_.Execute();
}

// ─── Reset ──────────────────────────────────────────────────────────────────

void ParallelSystemExecutor::Reset()
{
    // Clear local bookkeeping first, then reset the underlying graph.
    // Reversing the order would leave dangling NodeHandles in `entries_`
    // for a few cycles and invite a use-after-reset bug.
    entries_.clear();
    graph_.Reset();
}

} // namespace SagaEngine::ECS
