/// @file Renderer.cpp
/// @brief High-level Renderer — owns frame graph compilation and execution.

#include "SagaEngine/Render/Renderer.h"
#include "SagaEngine/RHI/IRHI.h"

namespace SagaEngine::Render
{

Renderer::~Renderer()
{
    if (m_initialized)
        Shutdown();
}

bool Renderer::Initialize()
{
    if (!m_rhi) return false;
    m_initialized = true;
    return true;
}

void Renderer::RenderFrame([[maybe_unused]] const Scene::RenderView& view)
{
    if (!m_initialized || !m_rhi) return;

    // Currently the real draw loop is in DiligentRenderBackend::Submit().
    // As we migrate to pass-based rendering, this method will:
    //   1. Build the RenderGraph (GBuffer → Lighting → PostProcess → UI).
    //   2. Compile it.
    //   3. Hand it to m_executor.
    //
    // For now this is intentionally empty — Submit() drives the frame.
}

void Renderer::Shutdown()
{
    m_executor.SetPasses({});
    m_initialized = false;
}

} // namespace SagaEngine::Render
