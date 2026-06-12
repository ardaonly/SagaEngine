/// @file MaterialRuntimeContract.h
/// @brief CPU-side material runtime validation and binding plan contract.

#pragma once

#include "SagaEngine/Render/Materials/Material.h"
#include "SagaEngine/Render/Shaders/ShaderPipelineContract.h"

#include <cstddef>
#include <vector>

namespace SagaEngine::Render
{

struct MaterialRuntimeBuildOptions
{
    ShaderTemplateHandle shaderHandle = ShaderTemplateHandle::kInvalid;
    TextureHandle missingTextureFallback = TextureHandle::kInvalid;
};

struct MaterialRuntimeBuildResult
{
    MaterialRuntime runtime{};
    std::vector<ShaderDiagnostic> diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept;
};

enum class MaterialParameterOverrideKind : std::uint8_t
{
    Scalar = 0,
    Vector,
};

struct MaterialParameterOverride
{
    MaterialParameterOverrideKind kind = MaterialParameterOverrideKind::Scalar;
    std::size_t index = 0;
    float scalarValue = 0.0f;
    MaterialVec4 vectorValue{};
};

struct MaterialBindingPlan
{
    ShaderTemplateHandle shaderHandle = ShaderTemplateHandle::kInvalid;
    MaterialRenderQueue renderQueue = MaterialRenderQueue::Opaque;
    MaterialCullMode cullMode = MaterialCullMode::Back;
    bool writesDepth = true;
    std::uint32_t boundTextureCount = 0;
    bool usesMissingTextureFallback = false;
};

[[nodiscard]] MaterialRuntime MakeDefaultMaterialRuntime(
    ShaderTemplateHandle shaderHandle,
    TextureHandle fallbackTexture) noexcept;

[[nodiscard]] MaterialRuntimeBuildResult BuildMaterialRuntime(
    const MaterialAsset& asset,
    const MaterialRuntimeBuildOptions& options);

[[nodiscard]] ShaderValidationResult ValidateMaterialAssetContract(
    const MaterialAsset& asset);

[[nodiscard]] ShaderValidationResult ApplyMaterialParameterOverride(
    MaterialRuntime& runtime,
    const MaterialParameterOverride& overrideValue);

[[nodiscard]] MaterialBindingPlan BuildMaterialBindingPlan(
    const MaterialRuntime& runtime,
    TextureHandle missingTextureFallback) noexcept;

} // namespace SagaEngine::Render
