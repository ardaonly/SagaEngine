/// @file DiligentGraphicsResourceRegistry.h
/// @brief Private handle registry used by the Diligent graphics adapter.

#pragma once

#include "SagaEngine/Graphics/Backend/GraphicsBackend.h"

#include <cstdint>
#include <vector>

namespace SagaEngine::Graphics::Backends::Diligent::Detail
{

struct NativeResourceOwnership
{
    std::uint64_t serial = 0;
    bool uploadDeferred = false;
};

template <typename HandleT, typename DescT>
class ResourceRegistry
{
public:
    explicit ResourceRegistry(std::uint32_t initialGeneration = 1u)
        : m_InitialGeneration(initialGeneration == 0u ? 1u
                                                      : initialGeneration)
    {
    }

    [[nodiscard]] HandleT Create(
        const DescT& desc,
        std::uint64_t estimatedBytes,
        const std::vector<std::uint8_t>& shadowPayload = {},
        GraphicsResourceBacking backing =
            GraphicsResourceBacking::RegisteredOnly,
        NativeResourceOwnership nativeOwnership = {},
        std::uint64_t creationSerial = 0,
        GraphicsResourceLifecycle lifecycle =
            GraphicsResourceLifecycle::RegisteredOnly)
    {
        std::uint32_t slotIndex = 0;
        if (!m_FreeSlots.empty())
        {
            slotIndex = m_FreeSlots.back();
            m_FreeSlots.pop_back();
            auto& slot = m_Slots[slotIndex];
            slot.desc = desc;
            slot.estimatedBytes = estimatedBytes;
            slot.shadowPayload = shadowPayload;
            slot.backing = backing;
            slot.nativeOwnership = nativeOwnership;
            slot.lifecycle = lifecycle;
            slot.creationSerial = creationSerial;
            slot.lastUseSerial = 0;
            slot.occupied = true;
            slot.generation = NextGeneration(slot.generation);
        }
        else
        {
            slotIndex = static_cast<std::uint32_t>(m_Slots.size());
            m_Slots.push_back(
                {
                    desc,
                    estimatedBytes,
                    shadowPayload,
                    backing,
                    nativeOwnership,
                    lifecycle,
                    creationSerial,
                    0u,
                    m_InitialGeneration,
                    true,
                });
        }

        m_LiveCount += 1u;
        m_LiveBytes += estimatedBytes;

        HandleT handle;
        handle.index = slotIndex + 1u;
        handle.generation = m_Slots[slotIndex].generation;
        return handle;
    }

    [[nodiscard]] bool UpdateNativeState(
        HandleT handle,
        GraphicsResourceBacking backing,
        NativeResourceOwnership nativeOwnership,
        GraphicsResourceLifecycle lifecycle) noexcept
    {
        if (!handle.IsValid() || handle.index > m_Slots.size())
        {
            return false;
        }

        auto& slot = m_Slots[handle.index - 1u];
        if (!slot.occupied || slot.generation != handle.generation)
        {
            return false;
        }

        slot.backing = backing;
        slot.nativeOwnership = nativeOwnership;
        slot.lifecycle = lifecycle;
        return true;
    }

    void Destroy(HandleT handle)
    {
        if (!handle.IsValid() || handle.index > m_Slots.size())
        {
            return;
        }

        const auto slotIndex = handle.index - 1u;
        auto& slot = m_Slots[slotIndex];
        if (!slot.occupied || slot.generation != handle.generation)
        {
            return;
        }

        slot.occupied = false;
        slot.shadowPayload.clear();
        slot.nativeOwnership = {};
        slot.lifecycle = GraphicsResourceLifecycle::Retired;
        slot.lastUseSerial = slot.creationSerial;
        m_LiveCount -= 1u;
        m_LiveBytes -= slot.estimatedBytes;
        m_FreeSlots.push_back(slotIndex);
    }

    void ReleaseAll()
    {
        m_FreeSlots.clear();
        for (std::uint32_t i = 0; i < m_Slots.size(); ++i)
        {
            auto& slot = m_Slots[i];
            slot.occupied = false;
            slot.shadowPayload.clear();
            slot.nativeOwnership = {};
            slot.lifecycle = GraphicsResourceLifecycle::Retired;
            slot.lastUseSerial = slot.creationSerial;
            m_FreeSlots.push_back(i);
        }
        m_LiveCount = 0;
        m_LiveBytes = 0;
    }

    [[nodiscard]] std::uint64_t ShadowBytes(HandleT handle)
        const noexcept
    {
        if (!handle.IsValid() || handle.index > m_Slots.size())
        {
            return 0u;
        }

        const auto slotIndex = handle.index - 1u;
        const auto& slot = m_Slots[slotIndex];
        if (!slot.occupied || slot.generation != handle.generation)
        {
            return 0u;
        }

        return static_cast<std::uint64_t>(slot.shadowPayload.size());
    }

    [[nodiscard]] std::uint64_t NativeSerial(HandleT handle)
        const noexcept
    {
        if (!handle.IsValid() || handle.index > m_Slots.size())
        {
            return 0u;
        }

        const auto slotIndex = handle.index - 1u;
        const auto& slot = m_Slots[slotIndex];
        if (!slot.occupied || slot.generation != handle.generation)
        {
            return 0u;
        }

        return slot.nativeOwnership.serial;
    }

    [[nodiscard]] bool NativeUploadDeferred(HandleT handle)
        const noexcept
    {
        if (!handle.IsValid() || handle.index > m_Slots.size())
        {
            return false;
        }

        const auto slotIndex = handle.index - 1u;
        const auto& slot = m_Slots[slotIndex];
        if (!slot.occupied || slot.generation != handle.generation)
        {
            return false;
        }

        return slot.nativeOwnership.uploadDeferred;
    }

    [[nodiscard]] std::uint32_t LiveCount() const noexcept
    {
        return m_LiveCount;
    }

    [[nodiscard]] std::uint64_t LiveBytes() const noexcept
    {
        return m_LiveBytes;
    }

    [[nodiscard]] GraphicsResourceBacking Backing(HandleT handle)
        const noexcept
    {
        if (!handle.IsValid() || handle.index > m_Slots.size())
        {
            return GraphicsResourceBacking::Invalid;
        }

        const auto slotIndex = handle.index - 1u;
        const auto& slot = m_Slots[slotIndex];
        if (!slot.occupied || slot.generation != handle.generation)
        {
            return GraphicsResourceBacking::Invalid;
        }

        return slot.backing;
    }

    [[nodiscard]] GraphicsResourceQueryResult Query(
        HandleT handle,
        GraphicsResourceKind kind) const
    {
        if (!handle.IsValid() || handle.index > m_Slots.size())
        {
            return {};
        }

        const auto slotIndex = handle.index - 1u;
        const auto& slot = m_Slots[slotIndex];
        if (!slot.occupied || slot.generation != handle.generation)
        {
            return {};
        }

        return {
            true,
            true,
            kind,
            slot.lifecycle,
            slot.backing,
            slot.backing == GraphicsResourceBacking::NativeGpu,
            slot.estimatedBytes,
            slot.estimatedBytes,
            slot.lifecycle == GraphicsResourceLifecycle::Ready &&
                slot.backing == GraphicsResourceBacking::NativeGpu,
            slot.lifecycle == GraphicsResourceLifecycle::PendingDestroy,
            slot.creationSerial,
            slot.lastUseSerial,
            slot.desc.debugName,
        };
    }

private:
    struct Slot
    {
        DescT desc{};
        std::uint64_t estimatedBytes = 0;
        std::vector<std::uint8_t> shadowPayload{};
        GraphicsResourceBacking backing =
            GraphicsResourceBacking::RegisteredOnly;
        NativeResourceOwnership nativeOwnership{};
        GraphicsResourceLifecycle lifecycle =
            GraphicsResourceLifecycle::Invalid;
        std::uint64_t creationSerial = 0;
        std::uint64_t lastUseSerial = 0;
        std::uint32_t generation = 1;
        bool occupied = false;
    };

    [[nodiscard]] static constexpr std::uint32_t NextGeneration(
        std::uint32_t generation) noexcept
    {
        return generation == 0xFFFFFFFFu ? 1u : generation + 1u;
    }

    std::vector<Slot> m_Slots;
    std::vector<std::uint32_t> m_FreeSlots;
    std::uint32_t m_InitialGeneration = 1;
    std::uint32_t m_LiveCount = 0;
    std::uint64_t m_LiveBytes = 0;
};

} // namespace SagaEngine::Graphics::Backends::Diligent::Detail
