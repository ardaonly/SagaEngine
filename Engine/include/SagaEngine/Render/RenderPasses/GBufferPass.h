/// @file GBufferPass.h
/// @brief Geometry pass that fills the G-buffer (albedo, normals, depth).

#pragma once

#include "SagaEngine/Render/RenderGraph/RGTypes.h"
#include "SagaEngine/Render/Scene/RenderView.h"

namespace SagaEngine::RHI { class IRHI; }

namespace SagaEngine::Render::Passes
{

/// Outputs written by the G-buffer pass.
struct GBufferOutputs
{
    RG::RGResourceId albedo   = RG::RGResourceId::kInvalid;
    RG::RGResourceId normals  = RG::RGResourceId::kInvalid;
    RG::RGResourceId depth    = RG::RGResourceId::kInvalid;
};

/// Draws all opaque geometry into the G-buffer textures.
/// Currently a placeholder — the real draw loop lives in
/// DiligentRenderBackend::Submit().  This will be wired in once
/// we migrate to the pass-based architecture.
class GBufferPass
{
public:
    /// Configure which RG resources this pass writes to.
    void SetOutputs(const GBufferOutputs& outputs) { m_outputs = outputs; }

    /// Execute the pass.
    void Execute(RHI::IRHI& rhi, const Scene::RenderView& view);

private:
    GBufferOutputs m_outputs{};
};

} // namespace SagaEngine::Render::Passes
