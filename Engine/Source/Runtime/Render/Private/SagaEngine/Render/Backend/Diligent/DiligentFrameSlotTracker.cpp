/// @file DiligentFrameSlotTracker.cpp
/// @brief Private frame-slot ownership contract for Diligent renderer resources.

#include "SagaEngine/Render/Backend/Diligent/DiligentFrameSlotTracker.h"

#include <algorithm>
#include <limits>

namespace SagaEngine::Render::Backend
{

bool DiligentFrameSlotTracker::Configure(
    std::uint32_t frameSlotCount,
    std::uint64_t bytesPerSlot)
{
    if (frameSlotCount == 0u)
    {
        Reset();
        return false;
    }

    m_Slots.assign(frameSlotCount, {});
    m_BytesPerSlot = bytesPerSlot;
    m_FrameIndex = 0;
    m_ActiveFrameSlot = 0;
    m_ActiveFrameSerial = 0;
    m_LastSubmittedSerial = 0;
    m_LastCompletedSerial = 0;
    m_ResetCount = 0;
    m_IncompleteSlotReuseCount = 0;
    m_DoubleBeginRejectCount = 0;
    m_OverflowCount = 0;
    m_ActiveFrameOpen = false;
    m_ActiveResetEligible = false;
    return true;
}

void DiligentFrameSlotTracker::Reset() noexcept
{
    m_Slots.clear();
    m_BytesPerSlot = 0;
    m_FrameIndex = 0;
    m_ActiveFrameSlot = 0;
    m_ActiveFrameSerial = 0;
    m_LastSubmittedSerial = 0;
    m_LastCompletedSerial = 0;
    m_ResetCount = 0;
    m_IncompleteSlotReuseCount = 0;
    m_DoubleBeginRejectCount = 0;
    m_OverflowCount = 0;
    m_ActiveFrameOpen = false;
    m_ActiveResetEligible = false;
}

DiligentFrameSlotBeginResult DiligentFrameSlotTracker::BeginFrame(
    std::uint64_t frameIndex,
    std::uint64_t completedSerial) noexcept
{
    if (m_Slots.empty())
    {
        return {};
    }
    if (m_ActiveFrameOpen)
    {
        ++m_DoubleBeginRejectCount;
        return {};
    }

    m_FrameIndex = frameIndex;
    m_ActiveFrameSlot =
        static_cast<std::uint32_t>(frameIndex % m_Slots.size());
    m_ActiveFrameSerial = 0;
    m_LastCompletedSerial =
        std::max(m_LastCompletedSerial, completedSerial);
    m_ActiveFrameOpen = true;

    auto& slot = m_Slots[m_ActiveFrameSlot];
    const auto resetEligible =
        SerialCompleted(m_LastCompletedSerial, slot.submittedSerial);
    m_ActiveResetEligible = resetEligible;
    if (resetEligible)
    {
        ResetSlotUsage(m_ActiveFrameSlot);
        ++m_ResetCount;
    }
    else
    {
        ++m_IncompleteSlotReuseCount;
    }

    return {
        true,
        m_FrameIndex,
        m_ActiveFrameSlot,
        m_LastCompletedSerial,
        slot.submittedSerial,
        resetEligible,
        !resetEligible,
        resetEligible ? 0u : slot.submittedSerial,
    };
}

void DiligentFrameSlotTracker::MarkWaitCompleted(
    std::uint64_t completedSerial) noexcept
{
    m_LastCompletedSerial =
        std::max(m_LastCompletedSerial, completedSerial);
    if (!m_ActiveFrameOpen || m_Slots.empty() || m_ActiveResetEligible)
    {
        return;
    }

    const auto& slot = m_Slots[m_ActiveFrameSlot];
    if (SerialCompleted(m_LastCompletedSerial, slot.submittedSerial))
    {
        ResetSlotUsage(m_ActiveFrameSlot);
        m_ActiveResetEligible = true;
        ++m_ResetCount;
    }
}

void DiligentFrameSlotTracker::BeginSubmission(
    std::uint64_t serial) noexcept
{
    if (!m_ActiveFrameOpen || serial == 0u)
    {
        return;
    }

    m_ActiveFrameSerial = serial;
}

void DiligentFrameSlotTracker::EndFrame() noexcept
{
    if (!m_ActiveFrameOpen || m_Slots.empty())
    {
        return;
    }

    auto& slot = m_Slots[m_ActiveFrameSlot];
    if (m_ActiveFrameSerial != 0u)
    {
        slot.submittedSerial = m_ActiveFrameSerial;
        m_LastSubmittedSerial = m_ActiveFrameSerial;
    }
    slot.mappingActive = false;
    m_ActiveFrameSerial = 0;
    m_ActiveFrameOpen = false;
    m_ActiveResetEligible = false;
}

bool DiligentFrameSlotTracker::BeginActiveMapping() noexcept
{
    if (!m_ActiveFrameOpen || m_Slots.empty())
    {
        return false;
    }

    auto& slot = m_Slots[m_ActiveFrameSlot];
    if (slot.mappingActive)
    {
        return false;
    }

    slot.mappingActive = true;
    return true;
}

void DiligentFrameSlotTracker::EndActiveMapping() noexcept
{
    if (m_ActiveFrameOpen && !m_Slots.empty())
    {
        m_Slots[m_ActiveFrameSlot].mappingActive = false;
    }
}

bool DiligentFrameSlotTracker::ReserveActiveBytes(
    std::uint64_t bytes) noexcept
{
    if (!m_ActiveFrameOpen || m_Slots.empty() || bytes == 0u)
    {
        return false;
    }

    auto& slot = m_Slots[m_ActiveFrameSlot];
    const auto max = std::numeric_limits<std::uint64_t>::max();
    if (slot.usedBytes > max - bytes ||
        (m_BytesPerSlot != 0u &&
         (slot.usedBytes > m_BytesPerSlot ||
          bytes > m_BytesPerSlot - slot.usedBytes)))
    {
        slot.overflowed = true;
        ++m_OverflowCount;
        return false;
    }

    slot.usedBytes += bytes;
    return true;
}

std::uint32_t DiligentFrameSlotTracker::FrameSlotCount() const noexcept
{
    return static_cast<std::uint32_t>(m_Slots.size());
}

std::uint32_t DiligentFrameSlotTracker::ActiveFrameSlot() const noexcept
{
    return m_ActiveFrameSlot;
}

std::uint64_t DiligentFrameSlotTracker::ActiveFrameSerial() const noexcept
{
    return m_ActiveFrameSerial;
}

bool DiligentFrameSlotTracker::ActiveFrameOpen() const noexcept
{
    return m_ActiveFrameOpen;
}

std::uint64_t DiligentFrameSlotTracker::LatestSubmittedSerial() const noexcept
{
    std::uint64_t latest = 0;
    for (const auto& slot : m_Slots)
    {
        latest = std::max(latest, slot.submittedSerial);
    }
    return latest;
}

std::uint64_t DiligentFrameSlotTracker::SlotSerial(
    std::uint32_t frameSlot) const noexcept
{
    return frameSlot < m_Slots.size() ? m_Slots[frameSlot].submittedSerial
                                      : 0u;
}

DiligentFrameSlotDiagnostics DiligentFrameSlotTracker::Diagnostics()
    const noexcept
{
    DiligentFrameSlotDiagnostics diagnostics{};
    diagnostics.frameSlotCount = FrameSlotCount();
    diagnostics.frameIndex = m_FrameIndex;
    diagnostics.activeFrameSlot = m_ActiveFrameSlot;
    diagnostics.activeFrameSerial = m_ActiveFrameSerial;
    diagnostics.lastSubmittedSerial = m_LastSubmittedSerial;
    diagnostics.lastCompletedSerial = m_LastCompletedSerial;
    diagnostics.resetCount = m_ResetCount;
    diagnostics.incompleteSlotReuseCount = m_IncompleteSlotReuseCount;
    diagnostics.doubleBeginRejectCount = m_DoubleBeginRejectCount;
    diagnostics.overflowCount = m_OverflowCount;
    diagnostics.activeSlotBudgetBytes = m_BytesPerSlot;
    diagnostics.activeFrameOpen = m_ActiveFrameOpen;

    if (!m_Slots.empty())
    {
        const auto& slot = m_Slots[m_ActiveFrameSlot];
        diagnostics.activeSlotUsedBytes = slot.usedBytes;
        diagnostics.activeMapping = slot.mappingActive;
        diagnostics.activeSlotOverflowed = slot.overflowed;
    }

    return diagnostics;
}

void DiligentFrameSlotTracker::ResetSlotUsage(
    std::uint32_t frameSlot) noexcept
{
    if (frameSlot >= m_Slots.size())
    {
        return;
    }

    auto& slot = m_Slots[frameSlot];
    slot.usedBytes = 0;
    slot.mappingActive = false;
    slot.overflowed = false;
}

} // namespace SagaEngine::Render::Backend
