/// @file DiligentRenderBackendFrame.cpp
/// @brief Per-frame begin/end bookkeeping for DiligentRenderBackend.

#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackendPrivate.h"

namespace SagaEngine::Render::Backend
{

void DiligentRenderBackend::BeginFrame()
{
    if (!m_Impl->initialized || !m_Impl->context || !m_Impl->swapChain)
        return;

    m_Impl->currentFrameDiagnostics = {};
    m_Impl->shadowSubmitAcceptedThisFrame = false;
    const auto completed = m_Impl->gpuTimeline.PollCompletion();
    m_Impl->nativeResources.RetireCompleted(completed);
    if (!m_Impl->frameSlotSerials.empty())
    {
        m_Impl->activeFrameSlot =
            static_cast<std::uint32_t>(
                m_Impl->frameIndex % m_Impl->frameSlotSerials.size());
        const auto slotSerial =
            m_Impl->frameSlotSerials[m_Impl->activeFrameSlot];
        if (slotSerial != 0u && completed < slotSerial)
        {
            m_Impl->gpuTimeline.WaitForSerial(slotSerial);
            m_Impl->nativeResources.RetireCompleted(
                m_Impl->gpuTimeline.LastCompletedSerial());
        }
    }
    m_Impl->activeFrameSerial = m_Impl->gpuTimeline.BeginFrameSubmission();
    m_Impl->currentFrameDiagnostics.gpuSubmissionSerial =
        m_Impl->activeFrameSerial;
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

    if (m_Impl->activeFrameSerial != 0u)
    {
        m_Impl->gpuTimeline.SignalFrameSubmitted(m_Impl->activeFrameSerial);
    }
    LogDbg("EndFrame: Present…");
    m_Impl->swapChain->Present();
    if (!m_Impl->frameSlotSerials.empty())
    {
        m_Impl->frameSlotSerials[m_Impl->activeFrameSlot] =
            m_Impl->activeFrameSerial;
    }
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
    m_Impl->activeFrameSerial = 0u;

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
