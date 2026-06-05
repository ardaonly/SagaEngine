/// @file FrameGraphExecutor.h
/// @brief Walks a compiled frame graph and executes each pass in order.

#pragma once

#include "SagaEngine/Render/RenderCore.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace SagaEngine::RHI { class IRHI; }

namespace SagaEngine::Render
{

/// A single scheduled pass inside the compiled frame graph.
struct ScheduledPass
{
    std::string                    debugName;
    RenderPassId                   id = RenderPassId::kInvalid;
    std::function<void(RHI::IRHI&)> execute;   ///< Callback that records commands.
};

/// Executes an ordered list of ScheduledPass entries against an RHI backend.
/// This is deliberately simple — the RenderGraph compiles into a flat list
/// and this class just walks it.
class FrameGraphExecutor
{
public:
    void SetPasses(std::vector<ScheduledPass> passes) { m_passes = std::move(passes); }

    /// Run every pass, in order, calling pass.execute(rhi).
    void Execute(RHI::IRHI& rhi);

    [[nodiscard]] std::uint32_t PassCount() const noexcept
    { return static_cast<std::uint32_t>(m_passes.size()); }

private:
    std::vector<ScheduledPass> m_passes;
};

} // namespace SagaEngine::Render
