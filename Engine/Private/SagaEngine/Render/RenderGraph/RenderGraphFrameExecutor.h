/// @file RenderGraphFrameExecutor.h
/// @brief Private frame lifecycle wrapper for RenderGraph execution.

#pragma once

#include "SagaEngine/Render/RenderGraph/RenderGraph.h"

#include <cstdint>

namespace SagaEngine::RHI
{
class IRHI;
}

namespace SagaEngine::Render::RG
{

enum class RGFrameExecutionSkipReason : std::uint8_t
{
    None = 0,
    InvalidFrame,
    GraphNotCompiled,
    InvalidCompile,
    CompiledStateInvalidated,
};

struct RGFrameExecutionDesc
{
    bool frameValid = true;
};

struct RGFrameExecutionResult
{
    bool attempted = false;
    bool submitted = false;
    RGFrameExecutionSkipReason skipReason =
        RGFrameExecutionSkipReason::GraphNotCompiled;
    std::uint32_t executedPassCount = 0;
};

class RenderGraphFrameExecutor
{
public:
    [[nodiscard]] RGFrameExecutionResult Execute(
        RenderGraph& graph,
        RHI::IRHI& rhi,
        const RGFrameExecutionDesc& desc = {}) const;
};

} // namespace SagaEngine::Render::RG
