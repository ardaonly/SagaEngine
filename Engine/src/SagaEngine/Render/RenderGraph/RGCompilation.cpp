/// @file RGCompilation.cpp
/// @brief Topological sort of render-graph passes by resource dependencies.

#include "SagaEngine/Render/RenderGraph/RGCompilation.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace SagaEngine::Render::RG
{

CompiledGraph RGCompiler::Compile(std::vector<RGPass>& passes)
{
    CompiledGraph result;

    // Build a map: resource → pass index that writes it.
    std::unordered_map<std::uint32_t, std::size_t> writerOf;
    for (std::size_t i = 0; i < passes.size(); ++i)
    {
        for (auto& out : passes[i].outputs)
        {
            auto id = static_cast<std::uint32_t>(out.resource);
            writerOf[id] = i;
        }
    }

    // Build adjacency: if pass[j] reads a resource that pass[i] writes,
    // then i → j (i must execute before j).
    std::vector<std::vector<std::size_t>> adj(passes.size());
    std::vector<std::uint32_t> inDeg(passes.size(), 0);

    for (std::size_t j = 0; j < passes.size(); ++j)
    {
        for (auto& in : passes[j].inputs)
        {
            auto id = static_cast<std::uint32_t>(in.resource);
            auto it = writerOf.find(id);
            if (it != writerOf.end() && it->second != j)
            {
                adj[it->second].push_back(j);
                ++inDeg[j];
            }
        }
    }

    // Kahn's algorithm — BFS topological sort.
    std::vector<std::size_t> queue;
    for (std::size_t i = 0; i < passes.size(); ++i)
        if (inDeg[i] == 0) queue.push_back(i);

    std::vector<std::size_t> order;
    order.reserve(passes.size());

    std::size_t front = 0;
    while (front < queue.size())
    {
        auto cur = queue[front++];
        order.push_back(cur);
        for (auto next : adj[cur])
        {
            if (--inDeg[next] == 0)
                queue.push_back(next);
        }
    }

    if (order.size() != passes.size())
    {
        // Cycle detected.
        result.valid = false;
        return result;
    }

    // Fill output.
    result.sortedPasses.reserve(order.size());
    for (std::size_t idx = 0; idx < order.size(); ++idx)
    {
        passes[order[idx]].order = static_cast<std::uint32_t>(idx);
        result.sortedPasses.push_back(&passes[order[idx]]);
    }
    result.valid = true;
    return result;
}

} // namespace SagaEngine::Render::RG
