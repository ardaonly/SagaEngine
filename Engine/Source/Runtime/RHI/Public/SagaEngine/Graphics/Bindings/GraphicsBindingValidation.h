/// @file GraphicsBindingValidation.h
/// @brief CPU-side vendor-neutral binding contract validation helpers.

#pragma once

#include "SagaEngine/Graphics/Bindings/GraphicsBindingTypes.h"

#include <cstdint>

namespace SagaEngine::Graphics
{

enum class GraphicsBindingValidationCode : std::uint8_t
{
    None = 0,
    DuplicateLayoutSlot,
    DuplicateBindingSlot,
    UnexpectedBindingSlot,
    MissingRequiredBinding,
    MissingFallbackBinding,
    InvalidHandle,
    StaleHandle,
    WrongResourceKind,
    MissingPairedSampler,
    DuplicateStableId,
    ZeroArrayCount,
    ArrayCountOutOfRange,
    EmptyStageMask,
    InvalidStageMask,
    UnsupportedBindingType,
    ArrayElementOutOfRange,
    InvalidBufferRange,
    StaleBindingLayout,
    ExplicitCompatibilityKeyMismatch,
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
    case GraphicsBindingType::StorageBuffer:
    default:
        return GraphicsResourceKind::Invalid;
    }
}

[[nodiscard]] constexpr bool IsSupportedGraphicsBindingType(
    GraphicsBindingType type) noexcept
{
    return type == GraphicsBindingType::Texture ||
           type == GraphicsBindingType::Buffer ||
           type == GraphicsBindingType::Sampler;
}

[[nodiscard]] GraphicsBindingValidationResult MakeGraphicsBindingValidationError(
    GraphicsBindingValidationCode code,
    std::uint32_t slot,
    GraphicsResourceKind expectedKind = GraphicsResourceKind::Invalid,
    GraphicsResourceKind actualKind = GraphicsResourceKind::Invalid);

[[nodiscard]] GraphicsBindingLayoutDesc NormalizeGraphicsBindingLayout(
    const GraphicsBindingLayoutDesc& layout);

/// Deterministic process-independent key for the current binding schema and
/// algorithm. It is a fast rejection/cache-bucketing value, not a collision-
/// proof identity or persisted cooked-asset ABI. Disk/cooked versioning belongs
/// with GFX-M8 / R6I.
[[nodiscard]] std::uint64_t ComputeGraphicsBindingLayoutKey(
    const GraphicsBindingLayoutDesc& layout);

[[nodiscard]] bool AreGraphicsBindingLayoutsCanonicallyEqual(
    const GraphicsBindingLayoutDesc& lhs,
    const GraphicsBindingLayoutDesc& rhs);

[[nodiscard]] bool AreGraphicsBindingLayoutsCompatible(
    const GraphicsBindingLayoutDesc& lhs,
    const GraphicsBindingLayoutDesc& rhs);

[[nodiscard]] bool AreGraphicsBindingLayoutsCompatible(
    const GraphicsBindingLayoutDesc& lhs,
    std::uint64_t lhsKey,
    const GraphicsBindingLayoutDesc& rhs,
    std::uint64_t rhsKey);

[[nodiscard]] GraphicsBindingValidationResult ValidateGraphicsBindingLayout(
    const GraphicsBindingLayoutDesc& layout);

[[nodiscard]] GraphicsBindingValidationResult ValidateGraphicsBindingSet(
    const GraphicsBindingLayoutDesc& layout,
    const GraphicsBindingSetDesc& bindingSet,
    GraphicsBindingResourceQueryFn queryResource,
    void* userData);

[[nodiscard]] std::uint32_t CountGraphicsBindingFallbackRequirements(
    const GraphicsBindingLayoutDesc& layout,
    const GraphicsBindingSetDesc& bindingSet);

} // namespace SagaEngine::Graphics
