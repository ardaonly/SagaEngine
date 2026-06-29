/// @file DiligentGraphicsBackendValidation.h
/// @brief Private validation and byte-estimation helpers for DiligentGraphicsBackend.

#pragma once

#include "SagaEngine/Graphics/Backend/GraphicsBackend.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceOwner.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace SagaEngine::Graphics::Backends::Diligent
{

[[nodiscard]] constexpr bool IsKnownFormat(ResourceFormat format) noexcept
{
    return format == ResourceFormat::Rgba8Unorm ||
           format == ResourceFormat::Rgba8UnormSrgb ||
           format == ResourceFormat::Bgra8Unorm ||
           format == ResourceFormat::Rgba16Float ||
           format == ResourceFormat::Rgba32Float ||
           format == ResourceFormat::Depth24Stencil8 ||
           format == ResourceFormat::Depth32Float;
}

[[nodiscard]] constexpr bool IsValidTextureDesc(
    const TextureDesc& desc) noexcept
{
    return desc.width != 0u && desc.height != 0u && desc.depth == 1u &&
           desc.mipLevels == 1u && desc.arrayLayers == 1u &&
           desc.dimension == TextureDimension::Texture2D &&
           (desc.format == ResourceFormat::Rgba8Unorm ||
            desc.format == ResourceFormat::Rgba8UnormSrgb) &&
           desc.usage == TextureUsageFlags::Sampled;
}

[[nodiscard]] constexpr bool IsValidBufferDesc(const BufferDesc& desc) noexcept
{
    return desc.sizeBytes != 0u &&
           ::SagaEngine::Render::Backend::DiligentNativeResourceOwner::IsBufferUsageSupported(
               desc.usage);
}

[[nodiscard]] constexpr bool IsValidShaderStage(ShaderStage stage) noexcept
{
    return stage == ShaderStage::Vertex || stage == ShaderStage::Fragment;
}

[[nodiscard]] constexpr bool IsValidShaderDesc(const ShaderDesc& desc) noexcept
{
    return (desc.byteSize != 0u || !desc.source.empty()) &&
           IsValidShaderStage(desc.stage) &&
           (desc.source.empty() || !desc.entryPoint.empty());
}

[[nodiscard]] constexpr bool IsValidTopology(
    PrimitiveTopology topology) noexcept
{
    return topology == PrimitiveTopology::TriangleList ||
           topology == PrimitiveTopology::TriangleStrip ||
           topology == PrimitiveTopology::LineList;
}

[[nodiscard]] constexpr bool IsValidPipelineDesc(
    const PipelineDesc& desc) noexcept
{
    return desc.colorTargetCount != 0u && desc.colorTargetCount <= 8u &&
           IsValidTopology(desc.topology) && IsKnownFormat(desc.colorFormat) &&
           IsKnownFormat(desc.depthFormat);
}

[[nodiscard]] constexpr bool IsValidFilterMode(FilterMode mode) noexcept
{
    return mode == FilterMode::Nearest || mode == FilterMode::Linear;
}

[[nodiscard]] constexpr bool IsValidAddressMode(AddressMode mode) noexcept
{
    return mode == AddressMode::ClampToEdge || mode == AddressMode::Repeat ||
           mode == AddressMode::MirrorRepeat;
}

[[nodiscard]] constexpr bool IsValidSamplerDesc(
    const SamplerDesc& desc) noexcept
{
    return IsValidFilterMode(desc.minFilter) &&
           IsValidFilterMode(desc.magFilter) &&
           IsValidAddressMode(desc.addressU) &&
           IsValidAddressMode(desc.addressV) &&
           IsValidAddressMode(desc.addressW);
}

[[nodiscard]] constexpr std::uint64_t BytesPerPixel(
    ResourceFormat format) noexcept
{
    switch (format)
    {
    case ResourceFormat::Rgba8Unorm:
    case ResourceFormat::Rgba8UnormSrgb:
    case ResourceFormat::Bgra8Unorm:
    case ResourceFormat::Depth24Stencil8:
    case ResourceFormat::Depth32Float:
        return 4u;
    case ResourceFormat::Rgba16Float:
        return 8u;
    case ResourceFormat::Rgba32Float:
        return 16u;
    case ResourceFormat::Unknown:
    default:
        return 0u;
    }
}

[[nodiscard]] constexpr std::uint64_t EstimateTextureBytes(
    const TextureDesc& desc) noexcept
{
    std::uint64_t totalPixels = 0;
    std::uint64_t width = desc.width;
    std::uint64_t height = desc.height;
    std::uint64_t depth = desc.depth;

    for (std::uint16_t mip = 0; mip < desc.mipLevels; ++mip)
    {
        totalPixels += width * height * depth * desc.arrayLayers;
        width = width > 1u ? width / 2u : 1u;
        height = height > 1u ? height / 2u : 1u;
        depth = depth > 1u ? depth / 2u : 1u;
    }

    return totalPixels * BytesPerPixel(desc.format);
}

[[nodiscard]] constexpr bool IsValidTextureInitialData(
    const TextureDesc& desc,
    GraphicsDataView initialData) noexcept
{
    if (initialData.sizeBytes == 0u)
    {
        return true;
    }

    if (!initialData.data)
    {
        return false;
    }

    const auto bytesPerPixel = BytesPerPixel(desc.format);
    if (bytesPerPixel == 0u)
    {
        return false;
    }

    if (desc.width >
        std::numeric_limits<std::uint64_t>::max() / bytesPerPixel)
    {
        return false;
    }
    const auto rowBytes =
        static_cast<std::uint64_t>(desc.width) * bytesPerPixel;
    if (initialData.rowPitchBytes != 0u &&
        initialData.rowPitchBytes < rowBytes)
    {
        return false;
    }

    const auto effectiveRowPitch =
        initialData.rowPitchBytes == 0u
            ? rowBytes
            : static_cast<std::uint64_t>(initialData.rowPitchBytes);
    if (desc.height != 0u &&
        effectiveRowPitch >
            std::numeric_limits<std::uint64_t>::max() /
                static_cast<std::uint64_t>(desc.height))
    {
        return false;
    }
    const auto sliceBytes =
        effectiveRowPitch * static_cast<std::uint64_t>(desc.height);
    if (initialData.slicePitchBytes != 0u &&
        initialData.slicePitchBytes < sliceBytes)
    {
        return false;
    }

    const auto effectiveSlicePitch =
        initialData.slicePitchBytes == 0u
            ? sliceBytes
            : static_cast<std::uint64_t>(initialData.slicePitchBytes);
    if (desc.depth != 0u &&
        effectiveSlicePitch >
            std::numeric_limits<std::uint64_t>::max() /
                static_cast<std::uint64_t>(desc.depth))
    {
        return false;
    }
    const auto requiredBytes =
        effectiveSlicePitch * static_cast<std::uint64_t>(desc.depth);

    return initialData.sizeBytes >= requiredBytes;
}

[[nodiscard]] constexpr std::uint64_t EstimateBufferBytes(
    const BufferDesc& desc) noexcept
{
    return desc.sizeBytes;
}

[[nodiscard]] constexpr bool IsValidBufferInitialData(
    const BufferDesc& desc,
    GraphicsDataView initialData) noexcept
{
    if (initialData.sizeBytes == 0u)
    {
        return true;
    }

    return initialData.data && initialData.sizeBytes <= desc.sizeBytes;
}

[[nodiscard]] inline std::vector<std::uint8_t> CopyShadowPayload(
    GraphicsDataView initialData)
{
    std::vector<std::uint8_t> payload;
    if (!initialData.data || initialData.sizeBytes == 0u)
    {
        return payload;
    }

    const auto* bytes = static_cast<const std::uint8_t*>(initialData.data);
    payload.assign(
        bytes,
        bytes + static_cast<std::size_t>(initialData.sizeBytes));
    return payload;
}

template <typename HandleT>
[[nodiscard]] constexpr HandleT ToTypedHandle(GraphicsHandle handle) noexcept
{
    HandleT typed{};
    typed.index = handle.index;
    typed.generation = handle.generation;
    return typed;
}

[[nodiscard]] constexpr std::uint64_t EstimateShaderBytes(
    const ShaderDesc& desc) noexcept
{
    return desc.source.empty()
               ? desc.byteSize
               : static_cast<std::uint64_t>(desc.source.size());
}

[[nodiscard]] constexpr std::uint64_t EstimatePipelineBytes(
    const PipelineDesc&) noexcept
{
    return 0u;
}

[[nodiscard]] constexpr std::uint64_t EstimateSamplerBytes(
    const SamplerDesc&) noexcept
{
    return 0u;
}

} // namespace SagaEngine::Graphics::Backends::Diligent
