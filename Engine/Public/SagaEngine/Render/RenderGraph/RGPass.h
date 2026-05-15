/// @file RGPass.h
/// @brief A single pass node inside the render graph.

#pragma once

#include "SagaEngine/Render/RenderGraph/RGTypes.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace SagaEngine::RHI { class IRHI; }

namespace SagaEngine::Render::RG
{

/// Describes one resource dependency for a pass.
struct RGResourceUsage
{
    RGResourceId resource = RGResourceId::kInvalid;
    RGAccess     access   = RGAccess::Read;
};

/// A render pass node.  During graph compilation the scheduler walks
/// these to determine execution order, resource lifetimes, and barriers.
struct RGPass
{
    std::string                     debugName;
    std::uint32_t                   order = 0;     ///< Topological order after compilation.
    std::vector<RGResourceUsage>    inputs;
    std::vector<RGResourceUsage>    outputs;
    std::function<void(RHI::IRHI&)> execute;       ///< The actual rendering callback.

    void AddInput(RGResourceId id, RGAccess access = RGAccess::Read)
    { inputs.push_back({id, access}); }

    void AddOutput(RGResourceId id, RGAccess access = RGAccess::Write)
    { outputs.push_back({id, access}); }
};

} // namespace SagaEngine::Render::RG
