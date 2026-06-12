/// @file FrameResourceAllocator.h
/// @brief Private CPU-side frame resource range allocator.

#pragma once

#include "SagaEngine/Graphics/Frame/FrameResources.h"

#include <cstdint>
#include <vector>

namespace SagaEngine::Graphics::Private
{

struct FrameResourceAllocatorConfig
{
    GraphicsFrameResourceClass resourceClass =
        GraphicsFrameResourceClass::PerFrameTransient;
    std::uint32_t maxFramesInFlight = 1;
    std::uint64_t bytesPerFrame = 0;
    std::uint64_t defaultAlignment = 16;
};

struct FrameResourceAllocation
{
    GraphicsFrameResourceClass resourceClass =
        GraphicsFrameResourceClass::PerFrameTransient;
    std::uint64_t frameIndex = 0;
    std::uint32_t frameSlot = 0;
    std::uint64_t offsetBytes = 0;
    std::uint64_t sizeBytes = 0;
    std::uint64_t alignmentBytes = 0;

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return sizeBytes != 0u;
    }
};

struct FrameResourceAllocatorStats
{
    GraphicsFrameResourceClass resourceClass =
        GraphicsFrameResourceClass::PerFrameTransient;
    std::uint64_t currentFrameIndex = 0;
    std::uint32_t currentFrameSlot = 0;
    std::uint64_t currentFrameBytes = 0;
    std::uint64_t peakFrameBytes = 0;
    std::uint64_t failedAllocationCount = 0;
    std::uint32_t maxFramesInFlight = 0;
};

class FrameResourceAllocator final
{
public:
    [[nodiscard]] bool Initialize(
        const FrameResourceAllocatorConfig& config)
    {
        if (config.maxFramesInFlight == 0u ||
            config.bytesPerFrame == 0u ||
            config.defaultAlignment == 0u)
        {
            return false;
        }

        m_Config = config;
        m_FrameBytes.assign(config.maxFramesInFlight, 0u);
        m_CurrentFrameIndex = 0;
        m_CurrentFrameSlot = 0;
        m_PeakFrameBytes = 0;
        m_FailedAllocationCount = 0;
        return true;
    }

    void BeginFrame(std::uint64_t frameIndex) noexcept
    {
        if (m_FrameBytes.empty())
        {
            return;
        }

        m_CurrentFrameIndex = frameIndex;
        m_CurrentFrameSlot =
            static_cast<std::uint32_t>(
                frameIndex % m_FrameBytes.size());
        m_FrameBytes[m_CurrentFrameSlot] = 0;
    }

    [[nodiscard]] FrameResourceAllocation Allocate(
        std::uint64_t sizeBytes,
        std::uint64_t alignmentBytes = 0) noexcept
    {
        if (m_FrameBytes.empty() || sizeBytes == 0u)
        {
            ++m_FailedAllocationCount;
            return {};
        }

        const auto alignment =
            alignmentBytes == 0u ? m_Config.defaultAlignment
                                 : alignmentBytes;
        if (alignment == 0u)
        {
            ++m_FailedAllocationCount;
            return {};
        }

        const auto currentBytes = m_FrameBytes[m_CurrentFrameSlot];
        const auto alignedOffset = AlignUp(currentBytes, alignment);
        if (alignedOffset > m_Config.bytesPerFrame ||
            sizeBytes > m_Config.bytesPerFrame - alignedOffset)
        {
            ++m_FailedAllocationCount;
            return {};
        }

        m_FrameBytes[m_CurrentFrameSlot] = alignedOffset + sizeBytes;
        if (m_FrameBytes[m_CurrentFrameSlot] > m_PeakFrameBytes)
        {
            m_PeakFrameBytes = m_FrameBytes[m_CurrentFrameSlot];
        }

        return {
            m_Config.resourceClass,
            m_CurrentFrameIndex,
            m_CurrentFrameSlot,
            alignedOffset,
            sizeBytes,
            alignment,
        };
    }

    [[nodiscard]] FrameResourceAllocatorStats GetStats() const noexcept
    {
        FrameResourceAllocatorStats stats{};
        stats.resourceClass = m_Config.resourceClass;
        stats.currentFrameIndex = m_CurrentFrameIndex;
        stats.currentFrameSlot = m_CurrentFrameSlot;
        stats.currentFrameBytes =
            m_FrameBytes.empty() ? 0u : m_FrameBytes[m_CurrentFrameSlot];
        stats.peakFrameBytes = m_PeakFrameBytes;
        stats.failedAllocationCount = m_FailedAllocationCount;
        stats.maxFramesInFlight = m_Config.maxFramesInFlight;
        return stats;
    }

    [[nodiscard]] GraphicsFrameResourceBudget GetBudget() const noexcept
    {
        const auto stats = GetStats();
        return {
            stats.resourceClass,
            stats.currentFrameBytes,
            stats.peakFrameBytes,
            stats.failedAllocationCount,
            stats.maxFramesInFlight,
        };
    }

    [[nodiscard]] std::uint64_t GetFrameBytesForTesting(
        std::uint32_t frameSlot) const noexcept
    {
        if (frameSlot >= m_FrameBytes.size())
        {
            return 0u;
        }

        return m_FrameBytes[frameSlot];
    }

private:
    [[nodiscard]] static constexpr std::uint64_t AlignUp(
        std::uint64_t value,
        std::uint64_t alignment) noexcept
    {
        const auto remainder = value % alignment;
        return remainder == 0u ? value : value + alignment - remainder;
    }

    FrameResourceAllocatorConfig m_Config{};
    std::vector<std::uint64_t> m_FrameBytes{};
    std::uint64_t m_CurrentFrameIndex = 0;
    std::uint32_t m_CurrentFrameSlot = 0;
    std::uint64_t m_PeakFrameBytes = 0;
    std::uint64_t m_FailedAllocationCount = 0;
};

} // namespace SagaEngine::Graphics::Private
