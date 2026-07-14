/// @file PipelineTypes.h
/// @brief Pipeline state descriptors for the RHI abstraction layer.

#pragma once

#include "SagaEngine/RHI/Types/ShaderTypes.h"
#include "SagaEngine/RHI/Types/TextureTypes.h"

#include <array>
#include <cstdint>
#include <string>

namespace SagaEngine::RHI
{

enum class CullMode : std::uint8_t { None = 0, Front = 1, Back = 2 };
enum class FillMode : std::uint8_t { Solid = 0, Wireframe = 1 };
enum class CompareOp : std::uint8_t { Never = 0, Less = 1, Equal = 2, LessEqual = 3, Greater = 4, Always = 7 };
enum class BlendFactor : std::uint8_t { Zero = 0, One = 1, SrcAlpha = 2, InvSrcAlpha = 3 };
enum class PrimitiveTopology : std::uint8_t { TriangleList = 0, TriangleStrip = 1, LineList = 2, PointList = 3 };

struct RasterizerDesc
{
    CullMode cullMode = CullMode::Back;
    FillMode fillMode = FillMode::Solid;
    bool     frontCCW = true;
};

struct DepthStencilDesc
{
    bool      depthEnable = true;
    bool      depthWrite  = true;
    CompareOp depthFunc   = CompareOp::Less;
};

struct BlendDesc
{
    bool        enable   = false;
    BlendFactor srcColor = BlendFactor::SrcAlpha;
    BlendFactor dstColor = BlendFactor::InvSrcAlpha;
    BlendFactor srcAlpha = BlendFactor::One;
    BlendFactor dstAlpha = BlendFactor::Zero;
};

struct GraphicsPipelineDesc
{
    std::string        debugName;
    ShaderHandle       vertexShader  = ShaderHandle::kInvalid;
    ShaderHandle       pixelShader   = ShaderHandle::kInvalid;
    RasterizerDesc     rasterizer{};
    DepthStencilDesc   depthStencil{};
    BlendDesc          blend{};
    PrimitiveTopology  topology = PrimitiveTopology::TriangleList;
    std::uint8_t       numRenderTargets = 1;
    std::array<TextureFormat, 8> rtvFormats{};
    TextureFormat      dsvFormat = TextureFormat::Depth24_S8;
};

struct ComputePipelineDesc
{
    std::string  debugName;
    ShaderHandle computeShader = ShaderHandle::kInvalid;
};

enum class PipelineHandle : std::uint32_t { kInvalid = 0 };

} // namespace SagaEngine::RHI
