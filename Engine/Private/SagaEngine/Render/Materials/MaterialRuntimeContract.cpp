#include <SagaEngine/Render/Materials/MaterialRuntimeContract.h>

#include <algorithm>
#include <utility>

namespace SagaEngine::Render
{

namespace
{

[[nodiscard]] bool IsError(const ShaderDiagnostic& diagnostic) noexcept
{
    return diagnostic.severity == ShaderDiagnosticSeverity::Error;
}

void AddDiagnostic(
    std::vector<ShaderDiagnostic>& diagnostics,
    std::string diagnosticId,
    std::string field,
    std::string message,
    ShaderDiagnosticSeverity severity = ShaderDiagnosticSeverity::Error)
{
    diagnostics.push_back(ShaderDiagnostic{
        severity,
        std::move(diagnosticId),
        std::move(field),
        std::move(message),
    });
}

[[nodiscard]] bool IsValidTextureSlot(MaterialTextureSlot slot) noexcept
{
    const auto index = static_cast<std::uint8_t>(slot);
    return index < kMaxTextureSlots;
}

[[nodiscard]] bool IsValidResidencyHint(
    MaterialTextureResidencyHint hint) noexcept
{
    return hint == MaterialTextureResidencyHint::Default ||
           hint == MaterialTextureResidencyHint::Streamed ||
           hint == MaterialTextureResidencyHint::AlwaysResident ||
           hint == MaterialTextureResidencyHint::PlaceholderAllowed;
}

} // namespace

bool MaterialRuntimeBuildResult::Succeeded() const noexcept
{
    return std::none_of(diagnostics.begin(), diagnostics.end(), IsError);
}

MaterialRuntime MakeDefaultMaterialRuntime(
    ShaderTemplateHandle shaderHandle,
    TextureHandle fallbackTexture) noexcept
{
    MaterialRuntime runtime = MakeErrorMaterial();
    runtime.shaderHandle = shaderHandle;
    runtime.textures.fill(fallbackTexture);
    return runtime;
}

ShaderValidationResult ValidateMaterialAssetContract(
    const MaterialAsset& asset)
{
    ShaderValidationResult result;
    if (asset.shaderTemplateName.empty())
    {
        AddDiagnostic(
            result.diagnostics,
            "Material.ShaderTemplateMissing",
            "shaderTemplateName",
            "Material asset must reference a shader template.");
    }
    if (asset.scalarParams.size() > kMaxScalarParams)
    {
        AddDiagnostic(
            result.diagnostics,
            "Material.ScalarParameterBudgetExceeded",
            "scalarParams",
            "Material scalar parameter count exceeds the runtime budget.");
    }
    if (asset.vectorParams.size() > kMaxVectorParams)
    {
        AddDiagnostic(
            result.diagnostics,
            "Material.VectorParameterBudgetExceeded",
            "vectorParams",
            "Material vector parameter count exceeds the runtime budget.");
    }
    if (asset.textures.size() > kMaxTextureSlots)
    {
        AddDiagnostic(
            result.diagnostics,
            "Material.TextureSlotBudgetExceeded",
            "textures",
            "Material texture count exceeds the runtime slot budget.");
    }
    for (const MaterialTextureRef& texture : asset.textures)
    {
        if (!IsValidTextureSlot(texture.slot))
        {
            AddDiagnostic(
                result.diagnostics,
                "Material.TextureSlotInvalid",
                "textures",
                "Material texture reference uses an invalid slot.");
        }
        if (!IsValidResidencyHint(texture.residencyHint))
        {
            AddDiagnostic(
                result.diagnostics,
                "Material.TextureResidencyHintInvalid",
                "textures",
                "Material texture residency hint is invalid.");
        }
        if (texture.assetId == 0 && texture.fallbackPath.empty())
        {
            AddDiagnostic(
                result.diagnostics,
                "Material.TextureFallbackMissing",
                "textures",
                "Material texture reference needs an asset id or fallback path.",
                ShaderDiagnosticSeverity::Warning);
        }
    }

    return result;
}

MaterialRuntimeBuildResult BuildMaterialRuntime(
    const MaterialAsset& asset,
    const MaterialRuntimeBuildOptions& options)
{
    MaterialRuntimeBuildResult result;
    result.runtime = MakeDefaultMaterialRuntime(
        options.shaderHandle,
        options.missingTextureFallback);
    result.runtime.materialId = asset.materialId;
    result.runtime.renderQueue = asset.renderQueue;
    result.runtime.cullMode = asset.cullMode;
    result.runtime.writesDepth = asset.writesDepth;
    result.runtime.receivesShadows = asset.receivesShadows;
    result.runtime.castsShadows = asset.castsShadows;
    result.runtime.alphaTestThreshold = asset.alphaTestThreshold;

    ShaderValidationResult validation = ValidateMaterialAssetContract(asset);
    result.diagnostics.insert(
        result.diagnostics.end(),
        validation.diagnostics.begin(),
        validation.diagnostics.end());

    if (options.shaderHandle == ShaderTemplateHandle::kInvalid)
    {
        AddDiagnostic(
            result.diagnostics,
            "Material.RuntimeShaderInvalid",
            "shaderHandle",
            "Material runtime build requires a resolved shader template handle.");
    }

    for (std::size_t index = 0;
         index < asset.scalarParams.size() && index < kMaxScalarParams;
         ++index)
    {
        result.runtime.scalars[index] = asset.scalarParams[index].value;
    }
    for (std::size_t index = 0;
         index < asset.vectorParams.size() && index < kMaxVectorParams;
         ++index)
    {
        result.runtime.vectors[index] = asset.vectorParams[index].value;
    }
    for (const MaterialTextureRef& texture : asset.textures)
    {
        if (!IsValidTextureSlot(texture.slot))
        {
            continue;
        }
        const auto slot = static_cast<std::size_t>(texture.slot);
        if (texture.assetId == 0)
        {
            result.runtime.textures[slot] = options.missingTextureFallback;
            if (options.missingTextureFallback == TextureHandle::kInvalid)
            {
                AddDiagnostic(
                    result.diagnostics,
                    "Material.MissingTextureFallbackInvalid",
                    "missingTextureFallback",
                    "Material has an unresolved texture and no valid fallback handle.");
            }
        }
        else
        {
            result.runtime.textures[slot] =
                static_cast<TextureHandle>(
                    static_cast<std::uint32_t>(texture.assetId));
        }
    }

    return result;
}

ShaderValidationResult ApplyMaterialParameterOverride(
    MaterialRuntime& runtime,
    const MaterialParameterOverride& overrideValue)
{
    ShaderValidationResult result;
    if (overrideValue.kind == MaterialParameterOverrideKind::Scalar)
    {
        if (overrideValue.index >= runtime.scalars.size())
        {
            AddDiagnostic(
                result.diagnostics,
                "Material.OverrideIndexInvalid",
                "index",
                "Scalar material override index is outside the runtime parameter block.");
            return result;
        }
        runtime.scalars[overrideValue.index] = overrideValue.scalarValue;
        return result;
    }

    if (overrideValue.index >= runtime.vectors.size())
    {
        AddDiagnostic(
            result.diagnostics,
            "Material.OverrideIndexInvalid",
            "index",
            "Vector material override index is outside the runtime parameter block.");
        return result;
    }
    runtime.vectors[overrideValue.index] = overrideValue.vectorValue;
    return result;
}

MaterialBindingPlan BuildMaterialBindingPlan(
    const MaterialRuntime& runtime,
    TextureHandle missingTextureFallback) noexcept
{
    MaterialBindingPlan plan;
    plan.shaderHandle = runtime.shaderHandle;
    plan.renderQueue = runtime.renderQueue;
    plan.cullMode = runtime.cullMode;
    plan.writesDepth = runtime.writesDepth;
    for (TextureHandle texture : runtime.textures)
    {
        if (texture != TextureHandle::kInvalid)
        {
            ++plan.boundTextureCount;
        }
        if (texture == missingTextureFallback &&
            missingTextureFallback != TextureHandle::kInvalid)
        {
            plan.usesMissingTextureFallback = true;
        }
    }
    return plan;
}

} // namespace SagaEngine::Render
