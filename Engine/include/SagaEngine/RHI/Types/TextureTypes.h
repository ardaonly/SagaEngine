/// @file TextureTypes.h
/// @brief GPU texture descriptors for the RHI abstraction layer.

#pragma once

#include <cstdint>
#include <string>

namespace SagaEngine::RHI
{

enum class TextureFormat : std::uint8_t
{
    Unknown     = 0,
    RGBA8_UNorm = 1,
    RGBA16_Float= 2,
    R32_Float   = 3,
    Depth24_S8  = 4,
    Depth32_Float = 5,
    BC1_UNorm   = 6,
    BC3_UNorm   = 7,
    BC7_UNorm   = 8,
};

enum class TextureDimension : std::uint8_t
{
    Tex2D      = 0,
    Tex3D      = 1,
    TexCube    = 2,
    Tex2DArray = 3,
};

enum class TextureUsageFlags : std::uint8_t
{
    ShaderResource = 1 << 0,
    RenderTarget   = 1 << 1,
    DepthStencil   = 1 << 2,
    UnorderedAccess= 1 << 3,
};

inline TextureUsageFlags operator|(TextureUsageFlags a, TextureUsageFlags b)
{
    return static_cast<TextureUsageFlags>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

inline bool HasFlag(TextureUsageFlags flags, TextureUsageFlags test)
{
    return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(test)) != 0;
}

struct TextureDesc
{
    std::string      debugName;
    std::uint32_t    width     = 1;
    std::uint32_t    height    = 1;
    std::uint32_t    depth     = 1;
    std::uint32_t    mipLevels = 1;
    std::uint32_t    arraySize = 1;
    TextureFormat    format    = TextureFormat::RGBA8_UNorm;
    TextureDimension dimension = TextureDimension::Tex2D;
    TextureUsageFlags usage    = TextureUsageFlags::ShaderResource;
};

enum class TextureViewHandle : std::uint32_t { kInvalid = 0 };

} // namespace SagaEngine::RHI
