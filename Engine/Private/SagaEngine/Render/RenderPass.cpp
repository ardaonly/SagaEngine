/// @file RenderPass.cpp
/// @brief GBufferPass and LightingPass stub implementations.

#include "SagaEngine/Render/RenderPasses/GBufferPass.h"
#include "SagaEngine/Render/RenderPasses/LightingPass.h"
#include "SagaEngine/RHI/IRHI.h"

namespace SagaEngine::Render::Passes
{

void GBufferPass::Execute([[maybe_unused]] RHI::IRHI& rhi,
                          [[maybe_unused]] const Scene::RenderView& view)
{
    // Stub — the real G-buffer fill will iterate view.drawItems,
    // bind per-material PSOs, and issue indexed draws.
    // For now the forward path in DiligentRenderBackend::Submit()
    // handles all drawing.
}

void LightingPass::Execute([[maybe_unused]] RHI::IRHI& rhi)
{
    // Stub — the deferred lighting pass will bind G-buffer SRVs,
    // set the full-screen triangle PSO, and dispatch a single draw.
    // Forward lighting currently lives in the pixel shader (kSolidPS).
}

} // namespace SagaEngine::Render::Passes
