/// @file GraphicsHandle.h
/// @brief Opaque public handles for Saga graphics resources.

#pragma once

#include <cstdint>

namespace SagaEngine::Graphics
{

struct GraphicsHandle
{
    std::uint32_t index      = 0;
    std::uint32_t generation = 0;

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return index != 0u;
    }
};

struct TextureHandle final : GraphicsHandle {};
struct BufferHandle final : GraphicsHandle {};
struct ShaderHandle final : GraphicsHandle {};
struct PipelineHandle final : GraphicsHandle {};
struct SamplerHandle final : GraphicsHandle {};

} // namespace SagaEngine::Graphics
