/// @file DiligentGpuTimeline.h
/// @brief Private CPU-wait fence timeline for Diligent frame ownership.

#pragma once

#include <cstdint>
#include <memory>

namespace Diligent
{
struct IDeviceContext;
struct IFence;
struct IRenderDevice;
}

namespace SagaEngine::Render::Backend
{

struct DiligentGpuTimelineDiagnostics
{
    std::uint64_t lastSubmittedSerial = 0;
    std::uint64_t lastCompletedSerial = 0;
    std::uint64_t targetedWaitCount = 0;
    std::uint64_t deviceWideWaitCount = 0;
    std::uint64_t pollCount = 0;
    std::uint64_t signalCount = 0;
};

class DiligentGpuTimeline final
{
public:
    DiligentGpuTimeline();
    ~DiligentGpuTimeline();

    DiligentGpuTimeline(const DiligentGpuTimeline&) = delete;
    DiligentGpuTimeline& operator=(const DiligentGpuTimeline&) = delete;

    [[nodiscard]] bool Initialize(
        Diligent::IRenderDevice* device,
        Diligent::IDeviceContext* context) noexcept;
    void Reset() noexcept;

    [[nodiscard]] std::uint64_t BeginFrameSubmission() noexcept;
    void SignalFrameSubmitted(std::uint64_t serial) noexcept;
    [[nodiscard]] std::uint64_t PollCompletion() noexcept;
    void WaitForSerial(std::uint64_t serial) noexcept;
    void ShutdownAndDrain() noexcept;

    [[nodiscard]] bool IsInitialized() const noexcept;
    [[nodiscard]] std::uint64_t LastSubmittedSerial() const noexcept;
    [[nodiscard]] std::uint64_t LastCompletedSerial() const noexcept;
    [[nodiscard]] DiligentGpuTimelineDiagnostics Diagnostics() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace SagaEngine::Render::Backend
