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

enum class DiligentBindingFailureReason : std::uint8_t
{
    None = 0,
    MissingLayout,
    MissingBindingSet,
    MissingPipeline,
    PipelineIncompatible,
    CanonicalLayoutMismatch,
    ResourceKindMismatch,
    StaleResource,
    ResourceGenerationMismatch,
    NativePayloadMissing,
    RequiredBindingMissing,
    UnsupportedBinding,
    UnsupportedArrayBinding,
    UnsupportedBufferRange,
    FallbackRequired,
    SrbCreationFailed,
    VariableLookupFailed,
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

struct DiligentNativeBindingDiagnostics
{
    std::uint64_t nativeBindingResolveAttempts = 0;
    std::uint64_t nativeBindingResolveSuccesses = 0;
    std::uint64_t nativeBindingResolveFailures = 0;
    std::uint64_t nativeBindingCacheHits = 0;
    std::uint64_t nativeBindingCacheMisses = 0;
    std::uint64_t nativeBindingCacheEntries = 0;
    std::uint64_t nativeBindingCacheInvalidations = 0;
    std::uint64_t nativeBindingSrbCreates = 0;
    std::uint64_t nativeBindingSrbCreateFailures = 0;
    std::uint64_t nativeBindingVariableLookups = 0;
    std::uint64_t nativeBindingVariableLookupFailures = 0;
    std::uint64_t staleLayoutRejects = 0;
    std::uint64_t staleBindingSetRejects = 0;
    std::uint64_t stalePipelineRejects = 0;
    std::uint64_t staleResourceRejects = 0;
    std::uint64_t resourceKindMismatchRejects = 0;
    std::uint64_t canonicalLayoutMismatchRejects = 0;
    std::uint64_t requiredBindingMissingRejects = 0;
    std::uint64_t unsupportedBindingRejects = 0;
    std::uint64_t fallbackTextureUses = 0;
    std::uint64_t fallbackSamplerUses = 0;
    std::uint64_t quarantinedSrbCount = 0;
    std::uint64_t quarantinedSrbPeak = 0;
};

[[nodiscard]] bool AreDiligentCompiledBindingLayoutsCompatible(
    const DiligentCompiledBindingLayout& lhs,
    const DiligentCompiledBindingLayout& rhs);

} // namespace SagaEngine::Graphics::Backends::Diligent
