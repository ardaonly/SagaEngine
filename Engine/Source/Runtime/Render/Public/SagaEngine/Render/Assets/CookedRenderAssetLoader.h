/// @file CookedRenderAssetLoader.h
/// @brief Vendor-neutral loaders for schema-version-1 cooked mesh and material artifacts.

#pragma once

#include "SagaEngine/Render/Materials/Material.h"
#include "SagaEngine/Render/Materials/MeshAsset.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEngine::Render
{

struct CookedRenderArtifactDiagnostic
{
    std::string diagnosticId;
    std::string message;
};

struct CookedMeshLoadResult
{
    MeshAsset asset{};
    std::vector<CookedRenderArtifactDiagnostic> diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept { return diagnostics.empty(); }
};

struct CookedMaterialLoadResult
{
    MaterialRuntime material{};
    std::vector<CookedRenderArtifactDiagnostic> diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept { return diagnostics.empty(); }
};

[[nodiscard]] CookedMeshLoadResult LoadCookedMeshArtifact(
    const std::filesystem::path& artifactPath);

[[nodiscard]] CookedMaterialLoadResult LoadCookedMaterialArtifact(
    const std::filesystem::path& artifactPath,
    ShaderTemplateHandle solidShaderHandle);

} // namespace SagaEngine::Render
