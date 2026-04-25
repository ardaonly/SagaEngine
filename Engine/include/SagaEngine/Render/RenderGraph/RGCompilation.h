/// @file RGCompilation.h
/// @brief Compiles a set of RGPass nodes into an execution schedule.

#pragma once

#include "SagaEngine/Render/RenderGraph/RGPass.h"

#include <vector>

namespace SagaEngine::Render::RG
{

/// Result of compiling the render graph — a topologically-sorted pass list
/// with resource lifetime info.
struct CompiledGraph
{
    std::vector<RGPass*> sortedPasses;   ///< Passes in execution order.
    bool                 valid = false;  ///< false if a cycle was detected.
};

/// Compiles a list of passes into a valid execution order.
/// Currently a simple topological sort on resource dependencies.
class RGCompiler
{
public:
    /// Build execution order.  Returns an invalid CompiledGraph if
    /// there is a cycle in the dependency graph.
    [[nodiscard]] CompiledGraph Compile(std::vector<RGPass>& passes);
};

} // namespace SagaEngine::Render::RG
