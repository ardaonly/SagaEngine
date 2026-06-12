/// @file ShaderVariantBudget.h
/// @brief Deterministic shader variant budget and material feature contracts.

#pragma once

#include "SagaEngine/Render/Shaders/ShaderPipelineContract.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace SagaEngine::Render
{

inline constexpr std::string_view kMaterialFeatureBaseColorTexture =
    "baseColorTexture";
inline constexpr std::string_view kMaterialFeatureNormalMap = "normalMap";
inline constexpr std::string_view kMaterialFeatureMraTexture = "mraTexture";
inline constexpr std::string_view kMaterialFeatureEmissive = "emissive";
inline constexpr std::string_view kMaterialFeatureAlphaTest = "alphaTest";
inline constexpr std::string_view kMaterialFeatureTransparent = "transparent";
inline constexpr std::string_view kMaterialFeatureDoubleSided = "doubleSided";
inline constexpr std::string_view kMaterialFeatureSkinning = "skinning";
inline constexpr std::string_view kMaterialFeatureInstancing = "instancing";
inline constexpr std::string_view kMaterialFeatureVertexColor = "vertexColor";

struct ShaderVariantBudgetConfig
{
    std::uint32_t maxVariantCount = 64;
    std::uint32_t maxFeatureFlagsPerMaterial = 8;
    std::uint64_t maxEstimatedCacheBytes = 8u * 1024u * 1024u;
    std::uint32_t maxEstimatedCompileMilliseconds = 30'000;
    std::string targetPlatform = "generic";
    std::string backend = "portable";
    bool deterministicPruning = true;
};

struct MaterialFeatureMatrix
{
    std::vector<std::string> allowedFeatures;
    std::vector<std::string> alwaysOnFeatures;
    std::vector<std::string> invalidPairs;
};

struct ShaderVariantBudgetCandidate
{
    ShaderVariantKey variant;
    std::vector<std::string> targetPlatforms;
    std::vector<std::string> backends;
    std::uint64_t estimatedCacheBytes = 0;
    std::uint32_t estimatedCompileMilliseconds = 0;
};

struct ShaderVariantBudgetReport
{
    std::uint32_t inputVariantCount = 0;
    std::uint32_t emittedVariantCount = 0;
    std::uint32_t prunedVariantCount = 0;
    std::uint64_t estimatedCacheBytes = 0;
    std::uint32_t estimatedCompileMilliseconds = 0;
    std::vector<ShaderVariantKey> emittedVariants;
    std::vector<ShaderDiagnostic> diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept;
};

[[nodiscard]] ShaderValidationResult ValidateMaterialFeatureMatrix(
    const MaterialFeatureMatrix& matrix);

[[nodiscard]] ShaderVariantBudgetReport EvaluateShaderVariantBudget(
    const ShaderVariantBudgetConfig& config,
    const std::vector<ShaderVariantBudgetCandidate>& candidates,
    const MaterialFeatureMatrix& matrix = {});

} // namespace SagaEngine::Render
