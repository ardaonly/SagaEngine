/// @file DiligentBindingRecords.h
/// @brief Private native binding metadata records for the Diligent adapter.

#pragma once

#include "SagaEngine/Graphics/Bindings/GraphicsBindingTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Graphics::Backends::Diligent
{

enum class DiligentBindingCompileStatus : std::uint8_t
{
    Invalid = 0,
    Compiled,
    Failed,
};

enum class DiligentCompiledBindingKind : std::uint8_t
{
    Unknown = 0,
    SampledTexture,
    ConstantBuffer,
    Sampler,
};

struct DiligentCompiledBindingEntry
{
    GraphicsBindingStableId stableId = 0;
    std::uint32_t slot = 0;
    DiligentCompiledBindingKind kind = DiligentCompiledBindingKind::Unknown;
    GraphicsBindingType bindingType = GraphicsBindingType::Texture;
    GraphicsShaderStageFlags stageMask = 0;
    std::uint32_t arrayCount = 1;
    GraphicsBindingFrequency frequency = GraphicsBindingFrequency::Material;
    bool required = true;
    GraphicsBindingFallbackPolicy fallbackPolicy =
        GraphicsBindingFallbackPolicy::None;
    std::string shaderVariableName;
    std::uint32_t diligentStageMask = 0;
    std::uint8_t diligentVariableType = 1;
};

struct DiligentCompiledBindingLayout
{
    std::uint32_t layoutHandleIndex = 0;
    std::uint32_t layoutHandleGeneration = 0;
    std::uint64_t compatibilityKey = 0;
    GraphicsBindingLayoutDesc canonicalLayout{};
    std::vector<DiligentCompiledBindingEntry> entries{};
    std::vector<std::string> diagnostics{};
    DiligentBindingCompileStatus status = DiligentBindingCompileStatus::Invalid;
};

struct DiligentNativeBindingResourceRecord
{
    GraphicsBindingStableId stableId = 0;
    std::uint32_t slot = 0;
    GraphicsResourceKind kind = GraphicsResourceKind::Invalid;
    GraphicsHandle handle{};
    std::uint32_t arrayElement = 0;
    std::uint64_t bufferOffsetBytes = 0;
    std::uint64_t bufferRangeBytes = 0;
    std::uint64_t resourceCreationSerial = 0;
    bool fallbackRequired = false;
    std::string shaderVariableName;
};

struct DiligentNativeBindingSetRecord
{
    std::uint32_t bindingSetHandleIndex = 0;
    std::uint32_t bindingSetHandleGeneration = 0;
    std::uint32_t layoutHandleIndex = 0;
    std::uint32_t layoutHandleGeneration = 0;
    std::uint64_t compatibilityKey = 0;
    GraphicsBindingLayoutDesc canonicalLayout{};
    std::vector<DiligentNativeBindingResourceRecord> resources{};
    std::vector<DiligentNativeBindingResourceRecord> fallbackRequirements{};
    std::vector<std::string> diagnostics{};
    DiligentBindingCompileStatus status = DiligentBindingCompileStatus::Invalid;
};

[[nodiscard]] bool AreDiligentCompiledBindingLayoutsCompatible(
    const DiligentCompiledBindingLayout& lhs,
    const DiligentCompiledBindingLayout& rhs);

} // namespace SagaEngine::Graphics::Backends::Diligent
