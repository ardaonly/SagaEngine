/// @file ShaderPipelineContract.h
/// @brief Saga-owned shader source, variant, cache, and diagnostic contracts.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace SagaEngine::Render
{

enum class ShaderStage : std::uint8_t
{
    Vertex = 0,
    Fragment,
    Compute,
};

enum class ShaderSourceLanguage : std::uint8_t
{
    Hlsl = 0,
    Glsl,
    Slang,
    Binary,
};

enum class ShaderIncludePolicy : std::uint8_t
{
    Disallow = 0,
    DeclaredOnly,
    ProjectRelative,
};

enum class ShaderOptimizationProfile : std::uint8_t
{
    Debug = 0,
    Development,
    Shipping,
};

enum class ShaderDiagnosticSeverity : std::uint8_t
{
    Info = 0,
    Warning,
    Error,
};

struct ShaderDiagnostic
{
    ShaderDiagnosticSeverity severity = ShaderDiagnosticSeverity::Error;
    std::string diagnosticId;
    std::string field;
    std::string message;
};

struct ShaderValidationResult
{
    std::vector<ShaderDiagnostic> diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept;
};

struct ShaderIncludeRef
{
    std::string logicalName;
    std::string path;
};

struct ShaderSourceDesc
{
    std::string debugName;
    std::string sourceId;
    std::string sourceText;
    std::string entryPoint;
    ShaderStage stage = ShaderStage::Vertex;
    ShaderSourceLanguage language = ShaderSourceLanguage::Hlsl;
    ShaderIncludePolicy includePolicy = ShaderIncludePolicy::DeclaredOnly;
    std::vector<ShaderIncludeRef> includes;
};

struct ShaderCompileProfile
{
    std::string profileName = "development";
    std::string targetPlatform = "generic";
    std::string backend = "portable";
    ShaderOptimizationProfile optimization =
        ShaderOptimizationProfile::Development;
    bool generateDebugInfo = false;
    std::string cacheNamespace = "render";
};

struct ShaderVariantFeature
{
    std::string name;
    std::string value = "1";
};

struct ShaderVariantKey
{
    std::string shaderId;
    std::vector<ShaderVariantFeature> features;
};

struct ShaderResourceBindingSummary
{
    std::string name;
    std::string resourceClass;
    std::uint32_t slot = 0;
    std::uint32_t count = 1;
};

struct ShaderReflectionSummary
{
    std::string entryPoint;
    ShaderStage stage = ShaderStage::Vertex;
    std::vector<std::string> inputSemantics;
    std::vector<std::string> outputSemantics;
    std::vector<ShaderResourceBindingSummary> resources;
    std::uint32_t constantBufferBytes = 0;
};

[[nodiscard]] bool IsValidShaderStage(ShaderStage stage) noexcept;
[[nodiscard]] bool IsValidShaderSourceLanguage(
    ShaderSourceLanguage language) noexcept;
[[nodiscard]] bool IsValidShaderIncludePolicy(
    ShaderIncludePolicy policy) noexcept;

[[nodiscard]] std::string_view ToString(ShaderStage stage) noexcept;
[[nodiscard]] std::string_view ToString(
    ShaderSourceLanguage language) noexcept;
[[nodiscard]] std::string_view ToString(
    ShaderIncludePolicy policy) noexcept;
[[nodiscard]] std::string_view ToString(
    ShaderOptimizationProfile profile) noexcept;

[[nodiscard]] ShaderValidationResult ValidateShaderSource(
    const ShaderSourceDesc& source);
[[nodiscard]] ShaderValidationResult ValidateShaderVariantKey(
    const ShaderVariantKey& variant);
[[nodiscard]] ShaderValidationResult ValidateShaderReflectionSummary(
    const ShaderReflectionSummary& reflection);

[[nodiscard]] std::string BuildShaderVariantStableString(
    const ShaderVariantKey& variant);
[[nodiscard]] std::uint64_t BuildShaderVariantStableHash(
    const ShaderVariantKey& variant);
[[nodiscard]] std::string BuildShaderCachePath(
    const ShaderSourceDesc& source,
    const ShaderCompileProfile& profile,
    const ShaderVariantKey& variant);

} // namespace SagaEngine::Render
