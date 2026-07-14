/// @file GraphicsBindingTypes.h
/// @brief Public vendor-neutral binding vocabulary.

#pragma once

#include "SagaEngine/Graphics/Core/GraphicsTypes.h"
#include "SagaEngine/Graphics/Handles/GraphicsHandle.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Graphics
{

inline constexpr std::uint32_t kInvalidGraphicsBindingSlot = 0xFFFFFFFFu;
inline constexpr std::uint32_t kMaxGraphicsBindingArrayCount = 1024u;

using GraphicsShaderStageFlags = std::uint32_t;
using GraphicsBindingStableId = std::uint64_t;

inline constexpr GraphicsShaderStageFlags kGraphicsShaderStageVertex =
    1u << 0u;
inline constexpr GraphicsShaderStageFlags kGraphicsShaderStageFragment =
    1u << 1u;
inline constexpr GraphicsShaderStageFlags kGraphicsShaderStageCompute =
    1u << 2u;

enum class GraphicsBindingType : std::uint8_t
{
    Texture = 0,
    SampledTexture = Texture,
    Buffer,
    ConstantBuffer = Buffer,
    Sampler,
    StorageBuffer,
};

enum class GraphicsBindingFrequency : std::uint8_t
{
    Static = 0,
    Material,
    Frame,
    Draw,
    Pass,
};

enum class GraphicsBindingFallbackPolicy : std::uint8_t
{
    None = 0,
    OptionalSampledTexture,
    OptionalSampler,
    OptionalConstantBuffer,
    OptionalShadowTexture,
};

struct GraphicsBindingLayoutSlot
{
    std::uint32_t slot = 0;
    GraphicsBindingStableId stableId = 0;
    GraphicsBindingType type = GraphicsBindingType::Texture;
    GraphicsShaderStageFlags stages = kGraphicsShaderStageFragment;
    std::uint32_t arrayCount = 1;
    GraphicsBindingFrequency frequency = GraphicsBindingFrequency::Material;
    bool required = true;
    std::uint32_t pairedSamplerSlot = kInvalidGraphicsBindingSlot;
    GraphicsBindingFallbackPolicy fallbackPolicy =
        GraphicsBindingFallbackPolicy::None;
};

struct GraphicsBindingLayoutDesc
{
    std::string debugName;
    std::vector<GraphicsBindingLayoutSlot> slots{};
};

struct GraphicsBindingResourceRef
{
    std::uint32_t slot = 0;
    GraphicsBindingStableId stableId = 0;
    GraphicsResourceKind kind = GraphicsResourceKind::Invalid;
    GraphicsHandle handle{};
    std::uint32_t arrayElement = 0;
    std::uint64_t bufferOffsetBytes = 0;
    std::uint64_t bufferRangeBytes = 0;
};

struct GraphicsBindingSetDesc
{
    std::string debugName;
    BindingLayoutHandle layout{};
    std::vector<GraphicsBindingResourceRef> resources{};
};

struct GraphicsBindingLayoutQueryResult
{
    bool valid = false;
    bool live = false;
    std::uint64_t compatibilityKey = 0;
    std::uint32_t bindingCount = 0;
    std::string debugName;
    GraphicsBindingLayoutDesc canonicalLayout{};
};

struct GraphicsBindingSetQueryResult
{
    bool valid = false;
    bool live = false;
    BindingLayoutHandle layout{};
    std::uint64_t compatibilityKey = 0;
    std::uint32_t bindingCount = 0;
    std::uint32_t fallbackRequiredCount = 0;
    std::string debugName;
};

} // namespace SagaEngine::Graphics
