/// @file ResourceDescs.h
/// @brief Scalar public descriptors for Saga graphics resources.

#pragma once

#include "SagaEngine/Graphics/Bindings/GraphicsBindingTypes.h"
#include "SagaEngine/Graphics/Core/GraphicsTypes.h"
#include "SagaEngine/Graphics/Handles/GraphicsHandle.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Graphics
{

struct TextureDesc
{
    std::string        debugName;
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
    std::string   debugName;
    std::uint64_t sizeBytes = 0;
    BufferUsage   usage     = BufferUsage::Vertex;
    bool          dynamic   = false;
};

struct ShaderDesc
{
    std::string   debugName;
    std::string   source;
    std::string   entryPoint = "main";
    std::string   sourceIdentity;
    ShaderStage   stage        = ShaderStage::Vertex;
    std::uint32_t byteSize     = 0;
    std::uint32_t variantKey   = 0;
    bool          debugSymbols = false;
};

struct VertexAttributeDesc
{
    std::uint32_t semanticIndex = 0;
    std::uint32_t bufferSlot = 0;
    std::uint32_t offsetBytes = 0;
    VertexElementFormat format = VertexElementFormat::Float32x3;
};

struct PipelineDesc
{
    std::string       debugName;
    ShaderHandle      vertexShader{};
    ShaderHandle      fragmentShader{};
    std::vector<VertexAttributeDesc> vertexLayout;
    PrimitiveTopology topology         = PrimitiveTopology::TriangleList;
    ResourceFormat    colorFormat      = ResourceFormat::Bgra8Unorm;
    ResourceFormat    depthFormat      = ResourceFormat::Depth24Stencil8;
    std::uint8_t      colorTargetCount = 1;
    std::uint8_t      sampleCount      = 1;
    std::uint64_t     bindingCompatibilityKey = 0;
    bool              depthTest        = true;
    bool              depthWrite       = true;
    bool              cullBackFaces    = true;
    bool              frontCounterClockwise = true;
    bool              alphaBlend       = false;
    BindingLayoutHandle bindingLayout{};
    GraphicsBindingLayoutDesc bindingCompatibilityLayout{};
};

struct SamplerDesc
{
    std::string debugName;
    FilterMode minFilter = FilterMode::Linear;
    FilterMode magFilter = FilterMode::Linear;
    AddressMode addressU = AddressMode::ClampToEdge;
    AddressMode addressV = AddressMode::ClampToEdge;
    AddressMode addressW = AddressMode::ClampToEdge;
    float mipLodBias     = 0.0f;
};

} // namespace SagaEngine::Graphics
