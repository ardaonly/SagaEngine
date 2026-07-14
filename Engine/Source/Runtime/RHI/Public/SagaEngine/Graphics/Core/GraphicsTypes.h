/// @file GraphicsTypes.h
/// @brief Saga-owned graphics vocabulary shared by public graphics headers.

#pragma once

#include <cstdint>
#include <string>

namespace SagaEngine::Graphics
{

enum class BackendPreference : std::uint8_t
{
    Auto           = 0,
    NativePrimary  = 1,
    NativePortable = 2,
    NativeLegacy   = 3,
    Compatibility  = 4,
};

enum class ResourceFormat : std::uint8_t
{
    Unknown = 0,
    Rgba8Unorm,
    Rgba8UnormSrgb,
    Bgra8Unorm,
    Rgba16Float,
    Rgba32Float,
    Depth24Stencil8,
    Depth32Float,
};

enum class TextureDimension : std::uint8_t
{
    Texture2D = 0,
    Texture3D,
    Cube,
};

enum class ShaderStage : std::uint8_t
{
    Vertex = 0,
    Fragment,
    Compute,
};

enum class PrimitiveTopology : std::uint8_t
{
    TriangleList = 0,
    TriangleStrip,
    LineList,
};

enum class FilterMode : std::uint8_t
{
    Nearest = 0,
    Linear,
};

enum class AddressMode : std::uint8_t
{
    ClampToEdge = 0,
    Repeat,
    MirrorRepeat,
};

enum class BufferUsage : std::uint8_t
{
    Vertex = 0,
    Index,
    Uniform,
    Storage,
};

enum class VertexElementFormat : std::uint8_t
{
    Float32x2 = 0,
    Float32x3,
    Float32x4,
    Uint8x4,
    Uint8x4Norm,
};

enum class TextureUsageFlags : std::uint16_t
{
    None         = 0,
    Sampled      = 1u << 0u,
    RenderTarget = 1u << 1u,
    DepthStencil = 1u << 2u,
    TransferSrc  = 1u << 3u,
    TransferDst  = 1u << 4u,
};

enum class GraphicsResourceBacking : std::uint8_t
{
    Invalid = 0,
    RegisteredOnly,
    NativeGpuFuture,
    NativeGpu,
};

enum class GraphicsResourceLifecycle : std::uint8_t
{
    Invalid = 0,
    RegisteredOnly,
    Ready,
    PendingDestroy,
    Retired,
    Failed,
};

enum class GraphicsResourceKind : std::uint8_t
{
    Invalid = 0,
    Texture,
    Buffer,
    Shader,
    Pipeline,
    Sampler,
};

struct GraphicsResourceQueryResult
{
    bool valid = false;
    bool live = false;
    GraphicsResourceKind kind = GraphicsResourceKind::Invalid;
    GraphicsResourceLifecycle lifecycle =
        GraphicsResourceLifecycle::Invalid;
    GraphicsResourceBacking backing = GraphicsResourceBacking::Invalid;
    bool nativeBacked = false;
    std::uint64_t logicalBytes = 0;
    std::uint64_t approximateBytes = 0;
    bool resident = false;
    bool pendingDestroy = false;
    std::uint64_t creationSerial = 0;
    std::uint64_t lastUseSerial = 0;
    std::string debugName;
};

[[nodiscard]] constexpr TextureUsageFlags operator|(
    TextureUsageFlags lhs,
    TextureUsageFlags rhs) noexcept
{
    return static_cast<TextureUsageFlags>(
        static_cast<std::uint16_t>(lhs) | static_cast<std::uint16_t>(rhs));
}

[[nodiscard]] constexpr bool HasFlag(
    TextureUsageFlags value,
    TextureUsageFlags flag) noexcept
{
    return (static_cast<std::uint16_t>(value) &
            static_cast<std::uint16_t>(flag)) != 0u;
}

} // namespace SagaEngine::Graphics
