/// @file DiligentGpuTimeline.cpp
/// @brief Private CPU-wait fence timeline for Diligent frame ownership.

#include "SagaEngine/Render/Backend/Diligent/DiligentGpuTimeline.h"

#include "RefCntAutoPtr.hpp"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "Fence.h"

namespace SagaEngine::Render::Backend
{

struct DiligentGpuTimeline::Impl
{
    Diligent::IDeviceContext* context = nullptr;
    Diligent::RefCntAutoPtr<Diligent::IFence> fence;
    std::uint64_t nextSerial = 1;
    DiligentGpuTimelineDiagnostics diagnostics{};
};

DiligentGpuTimeline::DiligentGpuTimeline()
    : m_Impl(std::make_unique<Impl>())
{
}

DiligentGpuTimeline::~DiligentGpuTimeline() = default;

bool DiligentGpuTimeline::Initialize(
    Diligent::IRenderDevice* device,
    Diligent::IDeviceContext* context) noexcept
{
    Reset();
    if (!device || !context)
    {
        return false;
    }

    Diligent::FenceDesc desc;
    desc.Name = "SagaFrameCompletionFence";
    desc.Type = Diligent::FENCE_TYPE_CPU_WAIT_ONLY;

    device->CreateFence(desc, &m_Impl->fence);
    if (!m_Impl->fence)
    {
        return false;
    }

    m_Impl->context = context;
    m_Impl->nextSerial = 1;
    m_Impl->diagnostics = {};
    return true;
}

void DiligentGpuTimeline::Reset() noexcept
{
    m_Impl->context = nullptr;
    m_Impl->fence.Release();
    m_Impl->nextSerial = 1;
    m_Impl->diagnostics = {};
}

std::uint64_t DiligentGpuTimeline::BeginFrameSubmission() noexcept
{
    return m_Impl->nextSerial++;
}

void DiligentGpuTimeline::SignalFrameSubmitted(std::uint64_t serial) noexcept
{
    if (!m_Impl->context || !m_Impl->fence || serial == 0u)
    {
        return;
    }

    m_Impl->context->EnqueueSignal(m_Impl->fence, serial);
    m_Impl->diagnostics.lastSubmittedSerial = serial;
    ++m_Impl->diagnostics.signalCount;
}

std::uint64_t DiligentGpuTimeline::PollCompletion() noexcept
{
    ++m_Impl->diagnostics.pollCount;
    if (!m_Impl->fence)
    {
        return m_Impl->diagnostics.lastCompletedSerial;
    }

    const auto completed =
        static_cast<std::uint64_t>(m_Impl->fence->GetCompletedValue());
    if (completed > m_Impl->diagnostics.lastCompletedSerial)
    {
        m_Impl->diagnostics.lastCompletedSerial = completed;
    }
    return m_Impl->diagnostics.lastCompletedSerial;
}

void DiligentGpuTimeline::WaitForSerial(std::uint64_t serial) noexcept
{
    if (!m_Impl->fence || serial == 0u ||
        m_Impl->diagnostics.lastCompletedSerial >= serial)
    {
        return;
    }

    m_Impl->fence->Wait(serial);
    ++m_Impl->diagnostics.targetedWaitCount;
    (void)PollCompletion();
}

void DiligentGpuTimeline::ShutdownAndDrain() noexcept
{
    if (m_Impl->fence && m_Impl->diagnostics.lastSubmittedSerial != 0u)
    {
        m_Impl->fence->Wait(m_Impl->diagnostics.lastSubmittedSerial);
        ++m_Impl->diagnostics.deviceWideWaitCount;
        (void)PollCompletion();
    }
}

bool DiligentGpuTimeline::IsInitialized() const noexcept
{
    return m_Impl->fence != nullptr && m_Impl->context != nullptr;
}

std::uint64_t DiligentGpuTimeline::LastSubmittedSerial() const noexcept
{
    return m_Impl->diagnostics.lastSubmittedSerial;
}

std::uint64_t DiligentGpuTimeline::LastCompletedSerial() const noexcept
{
    return m_Impl->diagnostics.lastCompletedSerial;
}

DiligentGpuTimelineDiagnostics DiligentGpuTimeline::Diagnostics()
    const noexcept
{
    return m_Impl->diagnostics;
}

} // namespace SagaEngine::Render::Backend
