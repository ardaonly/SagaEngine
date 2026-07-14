/// @file GraphicsBindingValidation.cpp
/// @brief CPU-side vendor-neutral binding contract validation implementation.

#include "SagaEngine/Graphics/Bindings/GraphicsBindingValidation.h"

#include <algorithm>
#include <cstddef>

namespace SagaEngine::Graphics
{

namespace
{

[[nodiscard]] const GraphicsBindingLayoutSlot* FindGraphicsBindingSlot(
    const GraphicsBindingLayoutDesc& layout,
    std::uint32_t slot) noexcept
{
    for (const auto& layoutSlot : layout.slots)
    {
        if (layoutSlot.slot == slot)
        {
            return &layoutSlot;
        }
    }

    return nullptr;
}

[[nodiscard]] const GraphicsBindingLayoutSlot* FindGraphicsBindingStableId(
    const GraphicsBindingLayoutDesc& layout,
    GraphicsBindingStableId stableId) noexcept
{
    if (stableId == 0u)
    {
        return nullptr;
    }

    for (const auto& layoutSlot : layout.slots)
    {
        if (layoutSlot.stableId == stableId)
        {
            return &layoutSlot;
        }
    }

    return nullptr;
}

[[nodiscard]] const GraphicsBindingResourceRef* FindGraphicsBindingResource(
    const GraphicsBindingSetDesc& bindingSet,
    std::uint32_t slot) noexcept
{
    for (const auto& resource : bindingSet.resources)
    {
        if (resource.slot == slot)
        {
            return &resource;
        }
    }

    return nullptr;
}

[[nodiscard]] const GraphicsBindingResourceRef*
FindGraphicsBindingResourceByStableId(
    const GraphicsBindingSetDesc& bindingSet,
    GraphicsBindingStableId stableId) noexcept
{
    if (stableId == 0u)
    {
        return nullptr;
    }

    for (const auto& resource : bindingSet.resources)
    {
        if (resource.stableId == stableId)
        {
            return &resource;
        }
    }

    return nullptr;
}

[[nodiscard]] const GraphicsBindingResourceRef*
FindGraphicsBindingResourceElement(
    const GraphicsBindingSetDesc& bindingSet,
    const GraphicsBindingLayoutSlot& layoutSlot,
    std::uint32_t arrayElement) noexcept
{
    for (const auto& resource : bindingSet.resources)
    {
        const bool matchesStableId =
            layoutSlot.stableId != 0u &&
            resource.stableId == layoutSlot.stableId;
        const bool matchesSlot =
            layoutSlot.stableId == 0u &&
            resource.slot == layoutSlot.slot;
        if ((matchesStableId || matchesSlot) &&
            resource.arrayElement == arrayElement)
        {
            return &resource;
        }
    }

    return nullptr;
}

void HashAppend(std::uint64_t& hash, std::uint64_t value) noexcept
{
    constexpr std::uint64_t prime = 1099511628211ull;
    for (std::uint32_t i = 0; i < 8u; ++i)
    {
        const auto byte =
            static_cast<std::uint8_t>((value >> (i * 8u)) & 0xFFu);
        hash ^= byte;
        hash *= prime;
    }
}

[[nodiscard]] bool AreGraphicsBindingSlotsCanonicallyEqual(
    const GraphicsBindingLayoutSlot& lhs,
    const GraphicsBindingLayoutSlot& rhs) noexcept
{
    return lhs.stableId == rhs.stableId && lhs.slot == rhs.slot &&
           lhs.type == rhs.type && lhs.stages == rhs.stages &&
           lhs.arrayCount == rhs.arrayCount &&
           lhs.frequency == rhs.frequency && lhs.required == rhs.required &&
           lhs.fallbackPolicy == rhs.fallbackPolicy;
}

[[nodiscard]] GraphicsBindingValidationResult ValidateGraphicsBindingHandle(
    std::uint32_t slot,
    GraphicsResourceKind expectedKind,
    GraphicsResourceKind declaredKind,
    GraphicsHandle handle,
    GraphicsBindingResourceQueryFn queryResource,
    void* userData)
{
    if (declaredKind != expectedKind)
    {
        return MakeGraphicsBindingValidationError(
            GraphicsBindingValidationCode::WrongResourceKind,
            slot,
            expectedKind,
            declaredKind);
    }

    if (!handle.IsValid())
    {
        return MakeGraphicsBindingValidationError(
            GraphicsBindingValidationCode::InvalidHandle,
            slot,
            expectedKind);
    }

    const auto query =
        queryResource ? queryResource(userData, declaredKind, handle)
                      : GraphicsResourceQueryResult{};
    if (!query.live || query.backing == GraphicsResourceBacking::Invalid)
    {
        return MakeGraphicsBindingValidationError(
            GraphicsBindingValidationCode::StaleHandle,
            slot,
            expectedKind,
            query.kind);
    }

    if (query.kind != expectedKind)
    {
        return MakeGraphicsBindingValidationError(
            GraphicsBindingValidationCode::WrongResourceKind,
            slot,
            expectedKind,
            query.kind);
    }

    return {};
}

} // namespace

GraphicsBindingValidationResult MakeGraphicsBindingValidationError(
    GraphicsBindingValidationCode code,
    std::uint32_t slot,
    GraphicsResourceKind expectedKind,
    GraphicsResourceKind actualKind)
{
    return {false, code, slot, expectedKind, actualKind};
}

GraphicsBindingLayoutDesc NormalizeGraphicsBindingLayout(
    const GraphicsBindingLayoutDesc& layout)
{
    GraphicsBindingLayoutDesc normalized = layout;
    std::sort(
        normalized.slots.begin(),
        normalized.slots.end(),
        [](const GraphicsBindingLayoutSlot& lhs,
           const GraphicsBindingLayoutSlot& rhs)
        {
            if (lhs.slot != rhs.slot)
            {
                return lhs.slot < rhs.slot;
            }
            return lhs.stableId < rhs.stableId;
        });
    return normalized;
}

std::uint64_t ComputeGraphicsBindingLayoutKey(
    const GraphicsBindingLayoutDesc& layout)
{
    constexpr std::uint64_t basis = 14695981039346656037ull;
    std::uint64_t hash = basis;
    const auto normalized = NormalizeGraphicsBindingLayout(layout);
    HashAppend(hash, normalized.slots.size());

    for (const auto& slot : normalized.slots)
    {
        HashAppend(hash, slot.stableId);
        HashAppend(hash, slot.slot);
        HashAppend(hash, static_cast<std::uint64_t>(slot.type));
        HashAppend(hash, slot.stages);
        HashAppend(hash, slot.arrayCount);
        HashAppend(hash, static_cast<std::uint64_t>(slot.frequency));
        HashAppend(hash, slot.required ? 1u : 0u);
        HashAppend(hash, static_cast<std::uint64_t>(slot.fallbackPolicy));
    }

    return hash == 0u ? 1u : hash;
}

bool AreGraphicsBindingLayoutsCanonicallyEqual(
    const GraphicsBindingLayoutDesc& lhs,
    const GraphicsBindingLayoutDesc& rhs)
{
    const auto normalizedLhs = NormalizeGraphicsBindingLayout(lhs);
    const auto normalizedRhs = NormalizeGraphicsBindingLayout(rhs);
    if (normalizedLhs.slots.size() != normalizedRhs.slots.size())
    {
        return false;
    }

    for (std::size_t i = 0u; i < normalizedLhs.slots.size(); ++i)
    {
        if (!AreGraphicsBindingSlotsCanonicallyEqual(
                normalizedLhs.slots[i],
                normalizedRhs.slots[i]))
        {
            return false;
        }
    }

    return true;
}

bool AreGraphicsBindingLayoutsCompatible(
    const GraphicsBindingLayoutDesc& lhs,
    const GraphicsBindingLayoutDesc& rhs)
{
    return AreGraphicsBindingLayoutsCompatible(
        lhs,
        ComputeGraphicsBindingLayoutKey(lhs),
        rhs,
        ComputeGraphicsBindingLayoutKey(rhs));
}

bool AreGraphicsBindingLayoutsCompatible(
    const GraphicsBindingLayoutDesc& lhs,
    std::uint64_t lhsKey,
    const GraphicsBindingLayoutDesc& rhs,
    std::uint64_t rhsKey)
{
    return lhsKey == rhsKey &&
           AreGraphicsBindingLayoutsCanonicallyEqual(lhs, rhs);
}

GraphicsBindingValidationResult ValidateGraphicsBindingLayout(
    const GraphicsBindingLayoutDesc& layout)
{
    for (std::size_t i = 0; i < layout.slots.size(); ++i)
    {
        const auto& slot = layout.slots[i];
        if (!IsSupportedGraphicsBindingType(slot.type))
        {
            return MakeGraphicsBindingValidationError(
                GraphicsBindingValidationCode::UnsupportedBindingType,
                slot.slot);
        }

        if (slot.arrayCount == 0u)
        {
            return MakeGraphicsBindingValidationError(
                GraphicsBindingValidationCode::ZeroArrayCount,
                slot.slot);
        }

        if (slot.arrayCount > kMaxGraphicsBindingArrayCount)
        {
            return MakeGraphicsBindingValidationError(
                GraphicsBindingValidationCode::ArrayCountOutOfRange,
                slot.slot);
        }

        if (slot.stages == 0u)
        {
            return MakeGraphicsBindingValidationError(
                GraphicsBindingValidationCode::EmptyStageMask,
                slot.slot);
        }

        constexpr GraphicsShaderStageFlags knownStages =
            kGraphicsShaderStageVertex | kGraphicsShaderStageFragment |
            kGraphicsShaderStageCompute;
        if ((slot.stages & ~knownStages) != 0u)
        {
            return MakeGraphicsBindingValidationError(
                GraphicsBindingValidationCode::InvalidStageMask,
                slot.slot);
        }

        for (std::size_t j = i + 1u; j < layout.slots.size(); ++j)
        {
            if (slot.slot == layout.slots[j].slot)
            {
                return MakeGraphicsBindingValidationError(
                    GraphicsBindingValidationCode::DuplicateLayoutSlot,
                    slot.slot);
            }

            if (slot.stableId != 0u &&
                slot.stableId == layout.slots[j].stableId)
            {
                return MakeGraphicsBindingValidationError(
                    GraphicsBindingValidationCode::DuplicateStableId,
                    slot.slot);
            }
        }
    }

    return {};
}

GraphicsBindingValidationResult ValidateGraphicsBindingSet(
    const GraphicsBindingLayoutDesc& layout,
    const GraphicsBindingSetDesc& bindingSet,
    GraphicsBindingResourceQueryFn queryResource,
    void* userData)
{
    const auto layoutResult = ValidateGraphicsBindingLayout(layout);
    if (!layoutResult.valid)
    {
        return layoutResult;
    }

    for (std::size_t i = 0; i < bindingSet.resources.size(); ++i)
    {
        for (std::size_t j = i + 1u; j < bindingSet.resources.size(); ++j)
        {
            if (bindingSet.resources[i].slot == bindingSet.resources[j].slot &&
                bindingSet.resources[i].arrayElement ==
                    bindingSet.resources[j].arrayElement)
            {
                return MakeGraphicsBindingValidationError(
                    GraphicsBindingValidationCode::DuplicateBindingSlot,
                    bindingSet.resources[i].slot);
            }

            if (bindingSet.resources[i].stableId != 0u &&
                bindingSet.resources[i].stableId ==
                    bindingSet.resources[j].stableId &&
                bindingSet.resources[i].arrayElement ==
                    bindingSet.resources[j].arrayElement)
            {
                return MakeGraphicsBindingValidationError(
                    GraphicsBindingValidationCode::DuplicateStableId,
                    bindingSet.resources[i].slot);
            }
        }
    }

    for (const auto& resource : bindingSet.resources)
    {
        const auto* layoutSlot =
            resource.stableId != 0u
                ? FindGraphicsBindingStableId(layout, resource.stableId)
                : FindGraphicsBindingSlot(layout, resource.slot);
        if (!layoutSlot)
        {
            return MakeGraphicsBindingValidationError(
                GraphicsBindingValidationCode::UnexpectedBindingSlot,
                resource.slot,
                GraphicsResourceKind::Invalid,
                resource.kind);
        }

        if (resource.arrayElement >= layoutSlot->arrayCount)
        {
            return MakeGraphicsBindingValidationError(
                GraphicsBindingValidationCode::ArrayElementOutOfRange,
                resource.slot,
                ToGraphicsResourceKind(layoutSlot->type),
                resource.kind);
        }

        if (layoutSlot->type != GraphicsBindingType::Buffer &&
            (resource.bufferOffsetBytes != 0u ||
             resource.bufferRangeBytes != 0u))
        {
            return MakeGraphicsBindingValidationError(
                GraphicsBindingValidationCode::InvalidBufferRange,
                resource.slot,
                ToGraphicsResourceKind(layoutSlot->type),
                resource.kind);
        }
    }

    for (const auto& layoutSlot : layout.slots)
    {
        const auto expectedKind = ToGraphicsResourceKind(layoutSlot.type);
        for (std::uint32_t arrayElement = 0;
             arrayElement < layoutSlot.arrayCount;
             ++arrayElement)
        {
            const auto* resource = FindGraphicsBindingResourceElement(
                bindingSet,
                layoutSlot,
                arrayElement);
            if (!resource)
            {
                if (layoutSlot.required)
                {
                    return MakeGraphicsBindingValidationError(
                        GraphicsBindingValidationCode::MissingRequiredBinding,
                        layoutSlot.slot,
                        expectedKind);
                }

                continue;
            }

            const auto resourceResult = ValidateGraphicsBindingHandle(
                layoutSlot.slot,
                expectedKind,
                resource->kind,
                resource->handle,
                queryResource,
                userData);
            if (!resourceResult.valid)
            {
                return resourceResult;
            }

            if (layoutSlot.type == GraphicsBindingType::Buffer)
            {
                const auto query =
                    queryResource
                        ? queryResource(userData, resource->kind, resource->handle)
                        : GraphicsResourceQueryResult{};
                if (resource->bufferOffsetBytes > query.logicalBytes ||
                    (resource->bufferRangeBytes != 0u &&
                     resource->bufferRangeBytes >
                         query.logicalBytes - resource->bufferOffsetBytes))
                {
                    return MakeGraphicsBindingValidationError(
                        GraphicsBindingValidationCode::InvalidBufferRange,
                        layoutSlot.slot,
                        expectedKind,
                        resource->kind);
                }
            }
        }
    }

    for (const auto& layoutSlot : layout.slots)
    {
        if (layoutSlot.type != GraphicsBindingType::Texture ||
            layoutSlot.pairedSamplerSlot == kInvalidGraphicsBindingSlot)
        {
            continue;
        }

        const auto* texture =
            layoutSlot.stableId != 0u
                ? FindGraphicsBindingResourceByStableId(
                      bindingSet,
                      layoutSlot.stableId)
                : FindGraphicsBindingResource(bindingSet, layoutSlot.slot);
        if (!texture)
        {
            continue;
        }

        const auto* sampler = FindGraphicsBindingResource(
            bindingSet,
            layoutSlot.pairedSamplerSlot);
        if (!sampler || sampler->kind != GraphicsResourceKind::Sampler)
        {
            return MakeGraphicsBindingValidationError(
                GraphicsBindingValidationCode::MissingPairedSampler,
                layoutSlot.slot,
                GraphicsResourceKind::Sampler,
                sampler ? sampler->kind : GraphicsResourceKind::Invalid);
        }
    }

    return {};
}

std::uint32_t CountGraphicsBindingFallbackRequirements(
    const GraphicsBindingLayoutDesc& layout,
    const GraphicsBindingSetDesc& bindingSet)
{
    std::uint32_t count = 0;
    for (const auto& layoutSlot : layout.slots)
    {
        if (layoutSlot.fallbackPolicy == GraphicsBindingFallbackPolicy::None)
        {
            continue;
        }

        const auto* resource =
            layoutSlot.stableId != 0u
                ? FindGraphicsBindingResourceByStableId(
                      bindingSet,
                      layoutSlot.stableId)
                : FindGraphicsBindingResource(bindingSet, layoutSlot.slot);
        if (!resource)
        {
            ++count;
        }
    }
    return count;
}

} // namespace SagaEngine::Graphics
