/// @file ShaderTypes.h
/// @brief Shader stage descriptors for the RHI abstraction layer.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::RHI
{

enum class ShaderStage : std::uint8_t
{
    Vertex   = 0,
    Pixel    = 1,
    Compute  = 2,
    Geometry = 3,
    Hull     = 4,
    Domain   = 5,
};

enum class ShaderSourceLang : std::uint8_t
{
    HLSL   = 0,
    GLSL   = 1,
    SPIRV  = 2,
};

struct ShaderDesc
{
    std::string     debugName;
    ShaderStage     stage      = ShaderStage::Vertex;
    ShaderSourceLang language  = ShaderSourceLang::HLSL;
    std::string     entryPoint = "main";
    std::string     source;                ///< Text source (HLSL/GLSL).
    std::vector<std::uint8_t> bytecode;    ///< Pre-compiled SPIRV / DXBC.
};

enum class ShaderHandle : std::uint32_t { kInvalid = 0 };

} // namespace SagaEngine::RHI
