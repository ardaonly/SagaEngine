/// @file ResourceDescs.h
/// @brief Scalar public descriptors for Saga graphics resources.

#pragma once

#include "SagaEngine/Graphics/Core/GraphicsTypes.h"

#include <cstdint>

namespace SagaEngine::Graphics
{

struct TextureDesc
{
    std::uint32_t      width       = 1;
    std::uint32_t      height      = 1;
    std::uint32_t      depth       = 1;
    std::uint16_t      mipLevels   = 1;
    std::uint16_t      arrayLayers = 1;
    ResourceFormat     format      = ResourceFormat::Rgba8Unorm;
    TextureDimension   dimension   = TextureDimension::Texture2D;
    TextureUsageFlags  usage       = TextureUsageFlags::Sampled;
};

struct BufferDesc
{
    std::uint64_t sizeBytes = 0;
    BufferUsage   usage     = BufferUsage::Vertex;
    bool          dynamic   = false;
};

struct ShaderDesc
{
    ShaderStage   stage        = ShaderStage::Vertex;
    std::uint32_t byteSize     = 0;
    std::uint32_t variantKey   = 0;
    bool          debugSymbols = false;
};

struct PipelineDesc
{
    PrimitiveTopology topology         = PrimitiveTopology::TriangleList;
    ResourceFormat    colorFormat      = ResourceFormat::Bgra8Unorm;
    ResourceFormat    depthFormat      = ResourceFormat::Depth24Stencil8;
    std::uint8_t      colorTargetCount = 1;
    bool              depthTest        = true;
    bool              depthWrite       = true;
};

struct SamplerDesc
{
    FilterMode minFilter = FilterMode::Linear;
    FilterMode magFilter = FilterMode::Linear;
    AddressMode addressU = AddressMode::ClampToEdge;
    AddressMode addressV = AddressMode::ClampToEdge;
    AddressMode addressW = AddressMode::ClampToEdge;
    float mipLodBias     = 0.0f;
};

} // namespace SagaEngine::Graphics
