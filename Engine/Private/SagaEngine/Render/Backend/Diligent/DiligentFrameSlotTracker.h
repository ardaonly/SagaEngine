/// @file DiligentFrameSlotTracker.h
/// @brief Private frame-slot ownership contract for Diligent renderer resources.

#pragma once

#include <cstdint>
#include <vector>

namespace SagaEngine::Render::Backend
{

struct DiligentFrameSlotBeginResult
{
    bool valid = false;
    std::uint64_t frameIndex = 0;
    std::uint32_t frameSlot = 0;
    std::uint64_t completedSerial = 0;
    std::uint64_t previousSlotSerial = 0;
    bool resetEligible = false;
    bool waitRequired = false;
    std::uint64_t waitSerial = 0;
};

struct DiligentFrameSlotDiagnostics
{
    std::uint32_t frameSlotCount = 0;
    std::uint64_t frameIndex = 0;
    std::uint32_t activeFrameSlot = 0;
    std::uint64_t activeFrameSerial = 0;
    std::uint64_t lastSubmittedSerial = 0;
    std::uint64_t lastCompletedSerial = 0;
    std::uint64_t resetCount = 0;
    std::uint64_t incompleteSlotReuseCount = 0;
    std::uint64_t doubleBeginRejectCount = 0;
    std::uint64_t overflowCount = 0;
    std::uint64_t activeSlotBudgetBytes = 0;
    std::uint64_t activeSlotUsedBytes = 0;
    bool activeFrameOpen = false;
    bool activeMapping = false;
    bool activeSlotOverflowed = false;
};

class DiligentFrameSlotTracker final
{
public:
    DiligentFrameSlotTracker() = default;
    DiligentFrameSlotTracker(const DiligentFrameSlotTracker&) = delete;
    DiligentFrameSlotTracker& operator=(const DiligentFrameSlotTracker&) =
        delete;
    DiligentFrameSlotTracker(DiligentFrameSlotTracker&&) = delete;
    DiligentFrameSlotTracker& operator=(DiligentFrameSlotTracker&&) = delete;

    [[nodiscard]] bool Configure(
        std::uint32_t frameSlotCount,
        std::uint64_t bytesPerSlot = 0);
    void Reset() noexcept;

    [[nodiscard]] DiligentFrameSlotBeginResult BeginFrame(
        std::uint64_t frameIndex,
        std::uint64_t completedSerial) noexcept;
    void MarkWaitCompleted(std::uint64_t completedSerial) noexcept;
    void BeginSubmission(std::uint64_t serial) noexcept;
    void EndFrame() noexcept;

    [[nodiscard]] bool BeginActiveMapping() noexcept;
    void EndActiveMapping() noexcept;
    [[nodiscard]] bool ReserveActiveBytes(std::uint64_t bytes) noexcept;

    [[nodiscard]] std::uint32_t FrameSlotCount() const noexcept;
    [[nodiscard]] std::uint32_t ActiveFrameSlot() const noexcept;
    [[nodiscard]] std::uint64_t ActiveFrameSerial() const noexcept;
    [[nodiscard]] bool ActiveFrameOpen() const noexcept;
    [[nodiscard]] std::uint64_t LatestSubmittedSerial() const noexcept;
    [[nodiscard]] std::uint64_t SlotSerial(
        std::uint32_t frameSlot) const noexcept;
    [[nodiscard]] DiligentFrameSlotDiagnostics Diagnostics()
        const noexcept;

private:
    struct Slot
    {
        std::uint64_t submittedSerial = 0;
        std::uint64_t usedBytes = 0;
        bool mappingActive = false;
        bool overflowed = false;
    };

    [[nodiscard]] static constexpr bool SerialCompleted(
        std::uint64_t completedSerial,
        std::uint64_t requiredSerial) noexcept
    {
        return requiredSerial == 0u || completedSerial >= requiredSerial;
    }

    void ResetSlotUsage(std::uint32_t frameSlot) noexcept;

    std::vector<Slot> m_Slots;
    std::uint64_t m_BytesPerSlot = 0;
    std::uint64_t m_FrameIndex = 0;
    std::uint32_t m_ActiveFrameSlot = 0;
    std::uint64_t m_ActiveFrameSerial = 0;
    std::uint64_t m_LastSubmittedSerial = 0;
    std::uint64_t m_LastCompletedSerial = 0;
    std::uint64_t m_ResetCount = 0;
    std::uint64_t m_IncompleteSlotReuseCount = 0;
    std::uint64_t m_DoubleBeginRejectCount = 0;
    std::uint64_t m_OverflowCount = 0;
    bool m_ActiveFrameOpen = false;
    bool m_ActiveResetEligible = false;
};

} // namespace SagaEngine::Render::Backend
