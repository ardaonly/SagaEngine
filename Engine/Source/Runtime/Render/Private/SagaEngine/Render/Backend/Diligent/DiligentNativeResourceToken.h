/// @file DiligentNativeResourceToken.h
/// @brief Private typed identity for runtime-owned native Diligent resources.

#pragma once

#include <cstdint>

namespace SagaEngine::Render::Backend
{

enum class DiligentNativeResourceKind : std::uint8_t
{
    Invalid,
    Buffer,
    Texture,
    Sampler,
    Shader,
    Pipeline,
};

struct DiligentNativeResourceToken
{
    DiligentNativeResourceKind kind = DiligentNativeResourceKind::Invalid;
    std::uint32_t index = 0;
    std::uint32_t generation = 0;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return kind != DiligentNativeResourceKind::Invalid &&
               index != 0u && generation != 0u;
    }
};

[[nodiscard]] constexpr bool operator==(
    DiligentNativeResourceToken lhs,
    DiligentNativeResourceToken rhs) noexcept
{
    return lhs.kind == rhs.kind &&
           lhs.index == rhs.index &&
           lhs.generation == rhs.generation;
}

using DiligentNativeBufferToken = DiligentNativeResourceToken;
using DiligentNativeTextureToken = DiligentNativeResourceToken;
using DiligentNativeSamplerToken = DiligentNativeResourceToken;
using DiligentNativeShaderToken = DiligentNativeResourceToken;
using DiligentNativePipelineToken = DiligentNativeResourceToken;

} // namespace SagaEngine::Render::Backend
