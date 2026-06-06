/// @file GraphicsBindingValidation.h
/// @brief CPU-side vendor-neutral binding validation helpers.

#pragma once

#include "SagaEngine/Graphics/Backend/GraphicsBackend.h"
#include "SagaEngine/Graphics/Handles/GraphicsHandle.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace SagaEngine::Graphics
{

inline constexpr std::uint32_t kInvalidGraphicsBindingSlot = 0xFFFFFFFFu;

using GraphicsShaderStageFlags = std::uint32_t;

inline constexpr GraphicsShaderStageFlags kGraphicsShaderStageVertex =
    1u << 0u;
inline constexpr GraphicsShaderStageFlags kGraphicsShaderStageFragment =
    1u << 1u;
inline constexpr GraphicsShaderStageFlags kGraphicsShaderStageCompute =
    1u << 2u;

enum class GraphicsBindingType : std::uint8_t
{
    Texture = 0,
    Buffer,
    Sampler,
};

struct GraphicsBindingLayoutSlot
{
    std::uint32_t slot = 0;
    GraphicsBindingType type = GraphicsBindingType::Texture;
    GraphicsShaderStageFlags stages = kGraphicsShaderStageFragment;
    bool required = true;
    std::uint32_t pairedSamplerSlot = kInvalidGraphicsBindingSlot;
};

struct GraphicsBindingLayoutDesc
{
    std::vector<GraphicsBindingLayoutSlot> slots{};
};

struct GraphicsBindingResourceRef
{
    std::uint32_t slot = 0;
    GraphicsResourceKind kind = GraphicsResourceKind::Invalid;
    GraphicsHandle handle{};
};

struct GraphicsBindingSetDesc
{
    std::vector<GraphicsBindingResourceRef> resources{};
};

enum class GraphicsBindingValidationCode : std::uint8_t
{
    None = 0,
    DuplicateLayoutSlot,
    DuplicateBindingSlot,
    MissingRequiredBinding,
    InvalidHandle,
    StaleHandle,
    WrongResourceKind,
    MissingPairedSampler,
};

struct GraphicsBindingValidationResult
{
    bool valid = true;
    GraphicsBindingValidationCode code = GraphicsBindingValidationCode::None;
    std::uint32_t slot = kInvalidGraphicsBindingSlot;
    GraphicsResourceKind expectedKind = GraphicsResourceKind::Invalid;
    GraphicsResourceKind actualKind = GraphicsResourceKind::Invalid;
};

using GraphicsBindingResourceQueryFn = GraphicsResourceQueryResult (*)(
    void* userData,
    GraphicsResourceKind kind,
    GraphicsHandle handle);

[[nodiscard]] constexpr GraphicsResourceKind ToGraphicsResourceKind(
    GraphicsBindingType type) noexcept
{
    switch (type)
    {
    case GraphicsBindingType::Texture:
        return GraphicsResourceKind::Texture;
    case GraphicsBindingType::Buffer:
        return GraphicsResourceKind::Buffer;
    case GraphicsBindingType::Sampler:
        return GraphicsResourceKind::Sampler;
    default:
        return GraphicsResourceKind::Invalid;
    }
}

[[nodiscard]] inline GraphicsBindingValidationResult
MakeGraphicsBindingValidationError(
    GraphicsBindingValidationCode code,
    std::uint32_t slot,
    GraphicsResourceKind expectedKind = GraphicsResourceKind::Invalid,
    GraphicsResourceKind actualKind = GraphicsResourceKind::Invalid)
{
    return {false, code, slot, expectedKind, actualKind};
}

[[nodiscard]] inline const GraphicsBindingLayoutSlot* FindGraphicsBindingSlot(
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

[[nodiscard]] inline const GraphicsBindingResourceRef*
FindGraphicsBindingResource(
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

[[nodiscard]] inline GraphicsBindingValidationResult
ValidateGraphicsBindingSet(
    const GraphicsBindingLayoutDesc& layout,
    const GraphicsBindingSetDesc& bindingSet,
    GraphicsBindingResourceQueryFn queryResource,
    void* userData)
{
    for (std::size_t i = 0; i < layout.slots.size(); ++i)
    {
        for (std::size_t j = i + 1u; j < layout.slots.size(); ++j)
        {
            if (layout.slots[i].slot == layout.slots[j].slot)
            {
                return MakeGraphicsBindingValidationError(
                    GraphicsBindingValidationCode::DuplicateLayoutSlot,
                    layout.slots[i].slot);
            }
        }
    }

    for (std::size_t i = 0; i < bindingSet.resources.size(); ++i)
    {
        for (std::size_t j = i + 1u; j < bindingSet.resources.size(); ++j)
        {
            if (bindingSet.resources[i].slot == bindingSet.resources[j].slot)
            {
                return MakeGraphicsBindingValidationError(
                    GraphicsBindingValidationCode::DuplicateBindingSlot,
                    bindingSet.resources[i].slot);
            }
        }
    }

    for (const auto& layoutSlot : layout.slots)
    {
        const auto* resource =
            FindGraphicsBindingResource(bindingSet, layoutSlot.slot);
        if (!resource)
        {
            if (layoutSlot.required)
            {
                return MakeGraphicsBindingValidationError(
                    GraphicsBindingValidationCode::MissingRequiredBinding,
                    layoutSlot.slot,
                    ToGraphicsResourceKind(layoutSlot.type));
            }

            continue;
        }

        const auto expectedKind = ToGraphicsResourceKind(layoutSlot.type);
        if (resource->kind != expectedKind)
        {
            return MakeGraphicsBindingValidationError(
                GraphicsBindingValidationCode::WrongResourceKind,
                layoutSlot.slot,
                expectedKind,
                resource->kind);
        }

        if (!resource->handle.IsValid())
        {
            return MakeGraphicsBindingValidationError(
                GraphicsBindingValidationCode::InvalidHandle,
                layoutSlot.slot,
                expectedKind);
        }

        const auto query =
            queryResource ? queryResource(userData, resource->kind,
                                          resource->handle)
                          : GraphicsResourceQueryResult{};
        if (!query.live || query.backing == GraphicsResourceBacking::Invalid)
        {
            return MakeGraphicsBindingValidationError(
                GraphicsBindingValidationCode::StaleHandle,
                layoutSlot.slot,
                expectedKind,
                query.kind);
        }

        if (query.kind != expectedKind)
        {
            return MakeGraphicsBindingValidationError(
                GraphicsBindingValidationCode::WrongResourceKind,
                layoutSlot.slot,
                expectedKind,
                query.kind);
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
            FindGraphicsBindingResource(bindingSet, layoutSlot.slot);
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

} // namespace SagaEngine::Graphics
