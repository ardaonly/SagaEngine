// SPDX-License-Identifier: MPL-2.0
/// @file DiligentRenderBackendOverlayApi.cpp
/// @brief Overlay and frame capture wrappers for DiligentRenderBackend.

#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackendPrivate.h"

namespace SagaEngine::Render::Backend
{

bool DiligentRenderBackend::InitOverlayRendering()
{
    if (!m_Impl || !m_Impl->runtime->IsInitialized()) return false;
    return m_Impl->overlayRenderer.Initialize(
        *m_Impl->runtime,
        m_Impl->runtime->FrameSlots().FrameSlotCount());
}

RenderOverlayTextureHandle DiligentRenderBackend::CreateOverlayTexture(
    const RenderOverlayTextureDesc& desc,
    const std::uint8_t* rgbaPixels)
{
    if (!m_Impl || !m_Impl->runtime->IsInitialized()) return {};
    if (!m_Impl->overlayRenderer.IsReady() && !InitOverlayRendering())
    {
        return {};
    }

    return m_Impl->overlayRenderer.CreateTexture(desc, rgbaPixels);
}

void DiligentRenderBackend::DestroyOverlayTexture(
    RenderOverlayTextureHandle texture)
{
    if (!m_Impl) return;
    m_Impl->overlayRenderer.DestroyTexture(texture);
}

void DiligentRenderBackend::RenderOverlayFrame(
    const ::SagaEngine::Render::Backend::RenderOverlayFrame& frame)
{
    if (!m_Impl || !m_Impl->runtime->IsInitialized()) return;
    m_Impl->overlayRenderer.Render(
        frame,
        m_Impl->runtime->FrameSlots().ActiveFrameSerial(),
        m_Impl->runtime->FrameSlots().ActiveFrameSlot());
}

void DiligentRenderBackend::ShutdownOverlayRendering()
{
    if (!m_Impl) return;
    m_Impl->overlayRenderer.Shutdown();
}

RenderOverlaySubmissionResult DiligentRenderBackend::InitializeOverlay()
{
    return InitOverlayRendering()
        ? RenderOverlaySubmissionResult{RenderOverlaySubmissionStatus::Success, {}}
        : RenderOverlaySubmissionResult{RenderOverlaySubmissionStatus::NotReady,
            "Diligent overlay initialization failed."};
}

RenderOverlayTextureHandle DiligentRenderBackend::CreateOverlayTextureResource(
    const RenderOverlayTextureDesc& desc, const std::uint8_t* rgbaPixels)
{
    return CreateOverlayTexture(desc, rgbaPixels);
}

void DiligentRenderBackend::DestroyOverlayTextureResource(RenderOverlayTextureHandle texture)
{
    DestroyOverlayTexture(texture);
}

RenderOverlaySubmissionResult DiligentRenderBackend::SubmitOverlayFrame(
    const ::SagaEngine::Render::Backend::RenderOverlayFrame& frame)
{
    if (!m_Impl || !m_Impl->runtime->IsInitialized())
        return {RenderOverlaySubmissionStatus::NotReady, "Diligent runtime is not initialized."};
    if (frame.displaySize[0] <= 0.0f || frame.displaySize[1] <= 0.0f)
        return {RenderOverlaySubmissionStatus::Minimized, "Overlay viewport is minimized."};
    if (!ValidateOverlayFrameForTests(frame))
        return {RenderOverlaySubmissionStatus::InvalidFrame, "Overlay draw list validation failed."};
    if (!m_Impl->overlayRenderer.IsReady() && !InitOverlayRendering())
        return {RenderOverlaySubmissionStatus::NotReady, "Diligent overlay is not ready."};
    this->RenderOverlayFrame(frame);
    return {RenderOverlaySubmissionStatus::Success, {}};
}

void DiligentRenderBackend::ShutdownOverlay()
{
    ShutdownOverlayRendering();
}

bool DiligentRenderBackend::CaptureCurrentColorFrame(
    RenderFrameCapture& outCapture)
{
    if (!m_Impl || !m_Impl->runtime->IsInitialized())
    {
        outCapture = {};
        return false;
    }

    return m_Impl->frameCapture.Capture(*m_Impl->runtime, outCapture) ==
        RenderCaptureResult::kSuccess;
}

// ─── Diagnostics ─────────────────────────────────────────────────────


bool InitBackendOverlayRendering(IRenderBackend& backend)
{
    auto* diligent = dynamic_cast<DiligentRenderBackend*>(&backend);
    return diligent && diligent->InitOverlayRendering();
}

RenderOverlayTextureHandle CreateBackendOverlayTexture(
    IRenderBackend& backend,
    const RenderOverlayTextureDesc& desc,
    const std::uint8_t* rgbaPixels)
{
    auto* diligent = dynamic_cast<DiligentRenderBackend*>(&backend);
    if (!diligent)
    {
        return {};
    }

    return diligent->CreateOverlayTexture(desc, rgbaPixels);
}

void DestroyBackendOverlayTexture(
    IRenderBackend& backend,
    RenderOverlayTextureHandle texture)
{
    auto* diligent = dynamic_cast<DiligentRenderBackend*>(&backend);
    if (diligent)
    {
        diligent->DestroyOverlayTexture(texture);
    }
}

void RenderBackendOverlayFrame(
    IRenderBackend& backend,
    const RenderOverlayFrame& frame)
{
    auto* diligent = dynamic_cast<DiligentRenderBackend*>(&backend);
    if (diligent)
    {
        diligent->RenderOverlayFrame(frame);
    }
}

void ShutdownBackendOverlayRendering(IRenderBackend& backend)
{
    auto* diligent = dynamic_cast<DiligentRenderBackend*>(&backend);
    if (diligent)
    {
        diligent->ShutdownOverlayRendering();
    }
}


} // namespace SagaEngine::Render::Backend
