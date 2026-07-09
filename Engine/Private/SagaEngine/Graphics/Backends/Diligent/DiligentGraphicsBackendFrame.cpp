/// @file DiligentGraphicsBackendFrame.cpp
/// @brief Frame status forwarding for the Diligent graphics adapter.

#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.h"

#include "SagaEngine/Graphics/Backend/GraphicsRenderBackendMapping.h"

namespace SagaEngine::Graphics::Backends::Diligent
{

namespace Mapping = ::SagaEngine::Graphics::Backend;

void DiligentGraphicsBackend::BeginFrame()
{
    if (m_Headless && m_HeadlessStatus.initialized)
    {
        ++m_HeadlessStatus.frameIndex;
        m_LastStatus = m_HeadlessStatus;
        return;
    }

    if (!CanRenderFrame())
    {
        SetFrameSkipped(m_LastStatus.failure);
        return;
    }

    if (m_Runtime)
    {
        (void)m_Runtime->BeginFrame(0u);
        m_LastStatus = GetStatus();
    }
}

void DiligentGraphicsBackend::EndFrame()
{
    if (!CanRenderFrame())
    {
        SetFrameSkipped(m_LastStatus.failure);
        return;
    }

    if (m_Runtime)
    {
        m_Runtime->PresentActiveFrame();
        m_LastStatus = GetStatus();
    }
}

RenderBackendStatus DiligentGraphicsBackend::GetStatus() const noexcept
{
    if (m_Headless)
    {
        return m_HeadlessStatus;
    }

    if (!m_Runtime || !m_LastStatus.initialized || m_SurfaceMinimized)
    {
        return m_LastStatus;
    }

    return Mapping::ToGraphicsBackendStatus(m_Runtime->Status());
}

RenderBackendCapabilities DiligentGraphicsBackend::GetCapabilities()
    const noexcept
{
    return m_LastCapabilities;
}

} // namespace SagaEngine::Graphics::Backends::Diligent
