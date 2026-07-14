#include <SagaEngine/Render/Assets/RenderAssetManifest.h>

#include <algorithm>
#include <sstream>
#include <unordered_set>
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

[[nodiscard]] std::string EscapeJson(std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value)
    {
        switch (ch)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }
    return escaped;
}

void WriteStringArray(
    std::ostringstream& output,
    const std::vector<std::string>& values)
{
    std::vector<std::string> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    output << '[';
    for (std::size_t index = 0; index < sorted.size(); ++index)
    {
        if (index != 0)
        {
            output << ',';
        }
        output << '"' << EscapeJson(sorted[index]) << '"';
    }
    output << ']';
}

template <typename T, typename IdFn>
[[nodiscard]] std::vector<T> SortedById(
    const std::vector<T>& values,
    IdFn idFn)
{
    std::vector<T> sorted = values;
    std::sort(
        sorted.begin(),
        sorted.end(),
        [&idFn](const T& lhs, const T& rhs)
        {
            return idFn(lhs) < idFn(rhs);
        });
    return sorted;
}

void ValidateCookedPath(
    std::vector<ShaderDiagnostic>& diagnostics,
    std::string_view id,
    std::string_view path,
    const RenderAssetManifestValidationOptions& options)
{
    if (id.empty())
    {
        AddDiagnostic(
            diagnostics,
            "RenderAssetManifest.AssetIdMissing",
            "assetId",
            "Render cook metadata must include a stable asset id.");
    }
    if (path.empty())
    {
        AddDiagnostic(
            diagnostics,
            "RenderAssetManifest.CookedPathMissing",
            "cookedPath",
            "Render cook metadata must include a cooked artifact path.");
        return;
    }
    if (options.requireCookedArtifactsPresent &&
        std::find(
            options.availableCookedPaths.begin(),
            options.availableCookedPaths.end(),
            path) == options.availableCookedPaths.end())
    {
        AddDiagnostic(
            diagnostics,
            "RenderAssetManifest.CookedAssetMissing",
            "cookedPath",
            "Cooked render asset path is not present in the available artifact set.");
    }
}

} // namespace

bool RenderAssetManifestValidationResult::Succeeded() const noexcept
{
    return std::none_of(diagnostics.begin(), diagnostics.end(), IsError);
}

RenderAssetManifestValidationResult ValidateRenderAssetManifest(
    const RenderAssetManifest& manifest,
    const RenderAssetManifestValidationOptions& options)
{
    RenderAssetManifestValidationResult result;
    if (manifest.schemaVersion != kRenderAssetManifestSchemaVersion)
    {
        AddDiagnostic(
            result.diagnostics,
            "RenderAssetManifest.SchemaVersionUnsupported",
            "schemaVersion",
            "Render asset manifest schema version is not supported.");
    }
    if (manifest.compatibilityVersion.empty())
    {
        AddDiagnostic(
            result.diagnostics,
            "RenderAssetManifest.CompatibilityVersionMissing",
            "compatibilityVersion",
            "Render asset manifest must include a compatibility version.");
    }

    std::unordered_set<std::string> ids;
    auto checkDuplicate = [&result, &ids](const std::string& id)
    {
        if (!id.empty() && !ids.insert(id).second)
        {
            AddDiagnostic(
                result.diagnostics,
                "RenderAssetManifest.AssetIdDuplicate",
                "assetId",
                "Render asset manifest contains a duplicate asset id.");
        }
    };

    for (const RenderTextureCookMetadata& texture : manifest.textures)
    {
        checkDuplicate(texture.assetId);
        ValidateCookedPath(
            result.diagnostics,
            texture.assetId,
            texture.cookedPath,
            options);
        if (texture.width == 0 || texture.height == 0)
        {
            AddDiagnostic(
                result.diagnostics,
                "RenderAssetManifest.TextureExtentInvalid",
                "textures",
                "Cooked texture metadata must include non-zero dimensions.");
        }
        if (texture.mipCount == 0)
        {
            AddDiagnostic(
                result.diagnostics,
                "RenderAssetManifest.TextureMipCountInvalid",
                "textures",
                "Cooked texture metadata must include at least one mip.");
        }
    }

    for (const RenderMeshCookMetadata& mesh : manifest.meshes)
    {
        checkDuplicate(mesh.assetId);
        ValidateCookedPath(
            result.diagnostics,
            mesh.assetId,
            mesh.cookedPath,
            options);
        if (mesh.vertexCount == 0 || mesh.indexCount == 0)
        {
            AddDiagnostic(
                result.diagnostics,
                "RenderAssetManifest.MeshGeometryInvalid",
                "meshes",
                "Cooked mesh metadata must include non-zero vertex and index counts.");
        }
    }

    for (const RenderMaterialCookMetadata& material : manifest.materials)
    {
        checkDuplicate(material.assetId);
        ValidateCookedPath(
            result.diagnostics,
            material.assetId,
            material.cookedPath,
            options);
        if (material.shaderTemplateName.empty())
        {
            AddDiagnostic(
                result.diagnostics,
                "RenderAssetManifest.MaterialShaderMissing",
                "materials",
                "Cooked material metadata must reference a shader template.");
        }
    }

    for (const RenderShaderVariantCookMetadata& shader : manifest.shaderVariants)
    {
        checkDuplicate(shader.assetId);
        ValidateCookedPath(
            result.diagnostics,
            shader.assetId,
            shader.cachePath,
            options);
        ShaderValidationResult variantValidation =
            ValidateShaderVariantKey(shader.variant);
        result.diagnostics.insert(
            result.diagnostics.end(),
            variantValidation.diagnostics.begin(),
            variantValidation.diagnostics.end());
    }

    return result;
}

std::string SerializeRenderAssetManifestJson(
    const RenderAssetManifest& manifest)
{
    std::ostringstream output;
    output << "{\n";
    output << "  \"schemaVersion\": " << manifest.schemaVersion << ",\n";
    output << "  \"compatibilityVersion\": \""
           << EscapeJson(manifest.compatibilityVersion) << "\",\n";

    output << "  \"textures\": [\n";
    const auto textures = SortedById(
        manifest.textures,
        [](const RenderTextureCookMetadata& value) { return value.assetId; });
    for (std::size_t index = 0; index < textures.size(); ++index)
    {
        const RenderTextureCookMetadata& texture = textures[index];
        output << "    {\"assetId\":\"" << EscapeJson(texture.assetId)
               << "\",\"contentHash\":\"" << EscapeJson(texture.contentHash)
               << "\",\"cookedPath\":\"" << EscapeJson(texture.cookedPath)
               << "\",\"estimatedBytes\":" << texture.estimatedBytes
               << ",\"format\":\"" << EscapeJson(texture.format)
               << "\",\"height\":" << texture.height
               << ",\"mipCount\":" << texture.mipCount
               << ",\"residencyHint\":\"" << EscapeJson(texture.residencyHint)
               << "\",\"sourcePath\":\"" << EscapeJson(texture.sourcePath)
               << "\",\"width\":" << texture.width << '}';
        output << (index + 1 == textures.size() ? "\n" : ",\n");
    }
    output << "  ],\n";

    output << "  \"meshes\": [\n";
    const auto meshes = SortedById(
        manifest.meshes,
        [](const RenderMeshCookMetadata& value) { return value.assetId; });
    for (std::size_t index = 0; index < meshes.size(); ++index)
    {
        const RenderMeshCookMetadata& mesh = meshes[index];
        output << "    {\"assetId\":\"" << EscapeJson(mesh.assetId)
               << "\",\"contentHash\":\"" << EscapeJson(mesh.contentHash)
               << "\",\"cookedPath\":\"" << EscapeJson(mesh.cookedPath)
               << "\",\"estimatedBytes\":" << mesh.estimatedBytes
               << ",\"hasTangents\":" << (mesh.hasTangents ? "true" : "false")
               << ",\"indexCount\":" << mesh.indexCount
               << ",\"lodCount\":" << mesh.lodCount
               << ",\"sourcePath\":\"" << EscapeJson(mesh.sourcePath)
               << "\",\"vertexCount\":" << mesh.vertexCount << '}';
        output << (index + 1 == meshes.size() ? "\n" : ",\n");
    }
    output << "  ],\n";

    output << "  \"materials\": [\n";
    const auto materials = SortedById(
        manifest.materials,
        [](const RenderMaterialCookMetadata& value) { return value.assetId; });
    for (std::size_t index = 0; index < materials.size(); ++index)
    {
        const RenderMaterialCookMetadata& material = materials[index];
        output << "    {\"assetId\":\"" << EscapeJson(material.assetId)
               << "\",\"compatibilityVersion\":\""
               << EscapeJson(material.compatibilityVersion)
               << "\",\"cookedPath\":\"" << EscapeJson(material.cookedPath)
               << "\",\"shaderTemplateName\":\""
               << EscapeJson(material.shaderTemplateName)
               << "\",\"shaderVariantIds\":";
        WriteStringArray(output, material.shaderVariantIds);
        output << ",\"textureAssetIds\":";
        WriteStringArray(output, material.textureAssetIds);
        output << '}';
        output << (index + 1 == materials.size() ? "\n" : ",\n");
    }
    output << "  ],\n";

    output << "  \"shaderVariants\": [\n";
    const auto shaderVariants = SortedById(
        manifest.shaderVariants,
        [](const RenderShaderVariantCookMetadata& value)
        {
            return value.assetId;
        });
    for (std::size_t index = 0; index < shaderVariants.size(); ++index)
    {
        const RenderShaderVariantCookMetadata& shader = shaderVariants[index];
        output << "    {\"assetId\":\"" << EscapeJson(shader.assetId)
               << "\",\"cachePath\":\"" << EscapeJson(shader.cachePath)
               << "\",\"estimatedCacheBytes\":"
               << shader.estimatedCacheBytes
               << ",\"estimatedCompileMilliseconds\":"
               << shader.estimatedCompileMilliseconds
               << ",\"reflectionPath\":\""
               << EscapeJson(shader.reflectionPath)
               << "\",\"stage\":\"" << ToString(shader.stage)
               << "\",\"variant\":\""
               << EscapeJson(BuildShaderVariantStableString(shader.variant))
               << "\"}";
        output << (index + 1 == shaderVariants.size() ? "\n" : ",\n");
    }
    output << "  ]\n";
    output << "}\n";
    return output.str();
}

} // namespace SagaEngine::Render
