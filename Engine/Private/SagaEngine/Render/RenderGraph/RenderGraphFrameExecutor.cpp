/// @file RenderGraphFrameExecutor.cpp
/// @brief Private frame lifecycle wrapper for RenderGraph execution.

#include "SagaEngine/Render/RenderGraph/RenderGraphFrameExecutor.h"

#include "SagaEngine/RHI/IRHI.h"

namespace SagaEngine::Render::RG
{

RGFrameExecutionResult RenderGraphFrameExecutor::Execute(
    RenderGraph& graph,
    RHI::IRHI& rhi,
    const RGFrameExecutionDesc& desc) const
{
    RGFrameExecutionResult result{};
    result.attempted = true;

    if (!desc.frameValid)
    {
        result.skipReason = RGFrameExecutionSkipReason::InvalidFrame;
        return result;
    }

    if (graph.IsCompiledStateDirty())
    {
        result.skipReason =
            RGFrameExecutionSkipReason::CompiledStateInvalidated;
        return result;
    }

    if (!graph.HasCompiledState())
    {
        result.skipReason = RGFrameExecutionSkipReason::GraphNotCompiled;
        return result;
    }

    if (!graph.IsCompiledStateValid())
    {
        result.skipReason = RGFrameExecutionSkipReason::InvalidCompile;
        return result;
    }

    rhi.BeginFrame();
    graph.Execute(rhi);
    rhi.EndFrame();

    const auto& execution = graph.GetLastExecutionSnapshot();
    result.submitted = execution.executed;
    result.skipReason = execution.executed
                            ? RGFrameExecutionSkipReason::None
                            : RGFrameExecutionSkipReason::InvalidCompile;
    result.executedPassCount = execution.executedPassCount;
    return result;
}

} // namespace SagaEngine::Render::RG
