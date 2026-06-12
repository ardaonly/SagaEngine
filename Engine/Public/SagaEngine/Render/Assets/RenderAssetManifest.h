/// @file RenderAssetManifest.h
/// @brief Render asset cooking metadata and deterministic manifest contract.

#pragma once

#include "SagaEngine/Render/Shaders/ShaderPipelineContract.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Render
{

inline constexpr std::uint32_t kRenderAssetManifestSchemaVersion = 1;

struct RenderTextureCookMetadata
{
    std::string assetId;
    std::string sourcePath;
    std::string cookedPath;
    std::string format;
    std::string residencyHint = "streamed";
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t mipCount = 1;
    std::uint64_t estimatedBytes = 0;
    std::string contentHash;
};

struct RenderMeshCookMetadata
{
    std::string assetId;
    std::string sourcePath;
    std::string cookedPath;
    std::uint32_t vertexCount = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t lodCount = 1;
    bool hasTangents = false;
    std::uint64_t estimatedBytes = 0;
    std::string contentHash;
};

struct RenderMaterialCookMetadata
{
    std::string assetId;
    std::string cookedPath;
    std::string shaderTemplateName;
    std::vector<std::string> textureAssetIds;
    std::vector<std::string> shaderVariantIds;
    std::string compatibilityVersion;
};

struct RenderShaderVariantCookMetadata
{
    std::string assetId;
    ShaderStage stage = ShaderStage::Vertex;
    ShaderVariantKey variant;
    std::string cachePath;
    std::string reflectionPath;
    std::uint64_t estimatedCacheBytes = 0;
    std::uint32_t estimatedCompileMilliseconds = 0;
};

struct RenderAssetManifest
{
    std::uint32_t schemaVersion = kRenderAssetManifestSchemaVersion;
    std::string compatibilityVersion = "render-cook-v1";
    std::vector<RenderTextureCookMetadata> textures;
    std::vector<RenderMeshCookMetadata> meshes;
    std::vector<RenderMaterialCookMetadata> materials;
    std::vector<RenderShaderVariantCookMetadata> shaderVariants;
};

struct RenderAssetManifestValidationOptions
{
    bool requireCookedArtifactsPresent = false;
    std::vector<std::string> availableCookedPaths;
};

struct RenderAssetManifestValidationResult
{
    std::vector<ShaderDiagnostic> diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept;
};

[[nodiscard]] RenderAssetManifestValidationResult ValidateRenderAssetManifest(
    const RenderAssetManifest& manifest,
    const RenderAssetManifestValidationOptions& options = {});

[[nodiscard]] std::string SerializeRenderAssetManifestJson(
    const RenderAssetManifest& manifest);

} // namespace SagaEngine::Render
