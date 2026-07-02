/// @file DiligentRenderBackendFrame.cpp
/// @brief Per-frame begin/end bookkeeping for DiligentRenderBackend.

#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackendPrivate.h"

namespace SagaEngine::Render::Backend
{

void DiligentRenderBackend::BeginFrame()
{
    if (!m_Impl->initialized || !m_Impl->context || !m_Impl->swapChain)
        return;
    if (m_Impl->frameSlots.ActiveFrameOpen())
    {
        LogErr("BeginFrame ignored: frame already active");
        return;
    }

    m_Impl->currentFrameDiagnostics = {};
    m_Impl->shadowSubmitAcceptedThisFrame = false;
    const auto completed = m_Impl->gpuTimeline.PollCompletion();
    m_Impl->nativeResources.RetireCompleted(completed);
    const auto slotBegin =
        m_Impl->frameSlots.BeginFrame(m_Impl->frameIndex, completed);
    if (!slotBegin.valid)
    {
        LogErr("BeginFrame rejected by frame slot tracker");
        return;
    }
    if (slotBegin.waitRequired)
    {
        m_Impl->gpuTimeline.WaitForSerial(slotBegin.waitSerial);
        m_Impl->nativeResources.RetireCompleted(
            m_Impl->gpuTimeline.LastCompletedSerial());
        m_Impl->frameSlots.MarkWaitCompleted(
            m_Impl->gpuTimeline.LastCompletedSerial());
    }
    const auto activeFrameSerial = m_Impl->gpuTimeline.BeginFrameSubmission();
    m_Impl->frameSlots.BeginSubmission(activeFrameSerial);
    m_Impl->currentFrameDiagnostics.gpuSubmissionSerial =
        activeFrameSerial;
    {
        const auto timeline = m_Impl->gpuTimeline.Diagnostics();
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

    LogDbg("BeginFrame: enter");

    auto* ctx = m_Impl->context.RawPtr();
    auto* sc  = m_Impl->swapChain.RawPtr();

    auto* rtv = sc->GetCurrentBackBufferRTV();
    auto* dsv = sc->GetDepthBufferDSV();

    if (g_verboseGPU)
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "BeginFrame: rtv=%p dsv=%p frame=%llu",
                      static_cast<void*>(rtv), static_cast<void*>(dsv),
                      static_cast<unsigned long long>(m_Impl->frameIndex));
        LogDbg(buf);
    }

    ctx->SetRenderTargets(
        rtv ? 1u : 0u, rtv ? &rtv : nullptr, dsv,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    LogDbg("BeginFrame: SetRenderTargets OK");

    if (rtv)
        ctx->ClearRenderTarget(rtv, m_Impl->config.clearColor,
                               Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    LogDbg("BeginFrame: ClearRenderTarget OK");

    if (dsv && !m_Impl->config.skipDepthClear)
        ctx->ClearDepthStencil(dsv,
                               Diligent::CLEAR_DEPTH_FLAG | Diligent::CLEAR_STENCIL_FLAG,
                               m_Impl->config.clearDepth, m_Impl->config.clearStencil,
                               Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    LogDbg("BeginFrame: done");
}


void DiligentRenderBackend::EndFrame()
{
    if (!m_Impl->initialized || !m_Impl->swapChain) return;
    if (!m_Impl->frameSlots.ActiveFrameOpen())
    {
        LogDbg("EndFrame ignored: no active frame");
        return;
    }

    const auto activeFrameSerial = m_Impl->frameSlots.ActiveFrameSerial();
    if (activeFrameSerial != 0u)
    {
        m_Impl->gpuTimeline.SignalFrameSubmitted(activeFrameSerial);
    }
    LogDbg("EndFrame: Present…");
    m_Impl->swapChain->Present();
    m_Impl->frameSlots.EndFrame();
    ++m_Impl->currentFrameDiagnostics.presentCalls;
    {
        const auto timeline = m_Impl->gpuTimeline.Diagnostics();
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
    ++m_Impl->frameIndex;

    if (g_verboseGPU)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "EndFrame: frame %llu done",
                      static_cast<unsigned long long>(m_Impl->frameIndex));
        LogDbg(buf);
    }
}

// ─── Overlay rendering ───────────────────────────────────────────────

} // namespace SagaEngine::Render::Backend
