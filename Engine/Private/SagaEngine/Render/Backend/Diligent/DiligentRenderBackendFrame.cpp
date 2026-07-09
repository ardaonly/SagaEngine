/// @file DiligentRenderBackendFrame.cpp
/// @brief Per-frame begin/end bookkeeping for DiligentRenderBackend.

#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackendPrivate.h"

namespace SagaEngine::Render::Backend
{

void DiligentRenderBackend::BeginFrame()
{
    if (!m_Impl->runtime->IsInitialized() || !m_Impl->runtime->Context() ||
        !m_Impl->runtime->SwapChain())
        return;
    if (m_Impl->runtime->FrameSlots().ActiveFrameOpen())
    {
        LOG_CAT_ERROR(Render, "BeginFrame ignored: frame already active");
        return;
    }

    m_Impl->currentFrameDiagnostics = {};
    m_Impl->shadowSubmitAcceptedThisFrame = false;
    auto frame = m_Impl->runtime->BeginFrame(0u);
    if (!frame.token.IsValid())
    {
        LOG_CAT_ERROR(Render, "BeginFrame rejected by frame slot tracker");
        return;
    }
    m_Impl->currentFrameDiagnostics.gpuSubmissionSerial =
        frame.token.submissionSerial;
    {
        const auto timeline = m_Impl->runtime->Timeline().Diagnostics();
        m_Impl->currentFrameDiagnostics.gpuCompletedSerial =
            timeline.lastCompletedSerial;
        m_Impl->currentFrameDiagnostics.gpuTargetedWaitCount =
            timeline.targetedWaitCount;
        m_Impl->currentFrameDiagnostics.gpuDeviceWideWaitCount =
            timeline.deviceWideWaitCount;
        m_Impl->currentFrameDiagnostics.gpuTimelinePollCount =
            timeline.pollCount;
        m_Impl->currentFrameDiagnostics.gpuTimelineSignalCount =
            timeline.signalCount;
    }

    LOG_CAT_DEBUG(Render, "BeginFrame: enter");

    auto* ctx = m_Impl->runtime->Context();
    auto* sc  = m_Impl->runtime->SwapChain();

    auto* rtv = sc->GetCurrentBackBufferRTV();
    auto* dsv = sc->GetDepthBufferDSV();

    if (m_Impl->gpuValidation)
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "BeginFrame: rtv=%p dsv=%p frame=%llu",
                      static_cast<void*>(rtv), static_cast<void*>(dsv),
                      static_cast<unsigned long long>(
                          m_Impl->runtime->Status().frameIndex));
        LOG_CAT_DEBUG(Render, buf);
    }

    ctx->SetRenderTargets(
        rtv ? 1u : 0u, rtv ? &rtv : nullptr, dsv,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    LOG_CAT_DEBUG(Render, "BeginFrame: SetRenderTargets OK");

    if (rtv)
        ctx->ClearRenderTarget(rtv, m_Impl->config.clearColor,
                               Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    LOG_CAT_DEBUG(Render, "BeginFrame: ClearRenderTarget OK");

    if (dsv && !m_Impl->config.skipDepthClear)
        ctx->ClearDepthStencil(dsv,
                               Diligent::CLEAR_DEPTH_FLAG | Diligent::CLEAR_STENCIL_FLAG,
                               m_Impl->config.clearDepth, m_Impl->config.clearStencil,
                               Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    LOG_CAT_DEBUG(Render, "BeginFrame: done");
}


void DiligentRenderBackend::EndFrame()
{
    if (!m_Impl->runtime->IsInitialized() || !m_Impl->runtime->SwapChain())
        return;
    if (!m_Impl->runtime->FrameSlots().ActiveFrameOpen())
    {
        LOG_CAT_DEBUG(Render, "EndFrame ignored: no active frame");
        return;
    }

    LOG_CAT_DEBUG(Render, "EndFrame: Present…");
    m_Impl->runtime->PresentActiveFrame();
    ++m_Impl->currentFrameDiagnostics.presentCalls;
    {
        const auto timeline = m_Impl->runtime->Timeline().Diagnostics();
        m_Impl->currentFrameDiagnostics.gpuCompletedSerial =
            timeline.lastCompletedSerial;
        m_Impl->currentFrameDiagnostics.gpuTargetedWaitCount =
            timeline.targetedWaitCount;
        m_Impl->currentFrameDiagnostics.gpuDeviceWideWaitCount =
            timeline.deviceWideWaitCount;
        m_Impl->currentFrameDiagnostics.gpuTimelinePollCount =
            timeline.pollCount;
        m_Impl->currentFrameDiagnostics.gpuTimelineSignalCount =
            timeline.signalCount;
    }
    m_Impl->lastFrameDiagnostics = m_Impl->currentFrameDiagnostics;

    if (m_Impl->gpuValidation)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "EndFrame: frame %llu done",
                      static_cast<unsigned long long>(
                          m_Impl->runtime->Status().frameIndex + 1u));
        LOG_CAT_DEBUG(Render, buf);
    }
}

// ─── Overlay rendering ───────────────────────────────────────────────

} // namespace SagaEngine::Render::Backend
