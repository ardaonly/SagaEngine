// SPDX-License-Identifier: MPL-2.0
#include "SagaEngine/Render/Assets/CookedRenderAssetLoader.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <openssl/evp.h>
#include <regex>
#include <sstream>

namespace SagaEngine::Render
{
namespace
{

[[nodiscard]] std::uint64_t StableId(std::string_view value) noexcept
{
    std::uint64_t hash = 14695981039346656037ull;
    for (const unsigned char character : value)
    {
        hash ^= character;
        hash *= 1099511628211ull;
    }
    return hash;
}

template <typename Result>
void Fail(Result& result, std::string id, std::string message)
{
    result.diagnostics.push_back({std::move(id), std::move(message)});
}

[[nodiscard]] bool IsSha256(const std::string& value)
{
    static const std::regex pattern("^sha256:[0-9a-f]{64}$");
    return std::regex_match(value, pattern);
}

[[nodiscard]] std::string Sha256(const std::string& bytes)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    auto* context = EVP_MD_CTX_new();
    if (context == nullptr || EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(context, bytes.data(), bytes.size()) != 1 ||
        EVP_DigestFinal_ex(context, digest.data(), &length) != 1)
    {
        if (context != nullptr) EVP_MD_CTX_free(context);
        return {};
    }
    EVP_MD_CTX_free(context);
    std::ostringstream output;
    output << "sha256:" << std::hex << std::setfill('0');
    for (unsigned int index = 0; index < length; ++index)
        output << std::setw(2) << static_cast<unsigned int>(digest[index]);
    return output.str();
}

template <typename Result>
[[nodiscard]] bool ReadArtifact(
    const std::filesystem::path& path,
    std::string_view expectedType,
    nlohmann::json& document,
    Result& result)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        Fail(result, "RenderArtifact.FileMissing", "Cooked render artifact cannot be opened.");
        return false;
    }
    try
    {
        input >> document;
    }
    catch (const nlohmann::json::exception& exception)
    {
        Fail(result, "RenderArtifact.ParseFailed", exception.what());
        return false;
    }
    if (document.value("schemaVersion", 0) != 1)
    {
        Fail(result, "RenderArtifact.SchemaUnsupported", "Cooked render artifact schemaVersion must be 1.");
        return false;
    }
    if (document.value("artifactType", std::string{}) != expectedType)
    {
        Fail(result, "RenderArtifact.TypeMismatch", "Cooked render artifact type does not match the requested loader.");
        return false;
    }
    if (!document.contains("artifactId") || !document["artifactId"].is_string() ||
        document["artifactId"].get<std::string>().empty() || !document.contains("payload"))
    {
        Fail(result, "RenderArtifact.IdentityInvalid", "Cooked render artifact identity or payload is missing.");
        return false;
    }
    const auto contentHash = document.value("contentHash", std::string{});
    const auto sourceHash = document.value("sourceHash", std::string{});
    if (!IsSha256(contentHash) || !IsSha256(sourceHash))
    {
        Fail(result, "RenderArtifact.HashMalformed", "Artifact hashes must use lowercase sha256:<64 hex> tokens.");
        return false;
    }
    const auto canonicalPayload = document["payload"].dump(2) + "\n";
    if (Sha256(canonicalPayload) != contentHash)
    {
        Fail(result, "RenderArtifact.ContentHashMismatch", "Artifact payload does not match contentHash.");
        return false;
    }
    return true;
}

} // namespace

CookedMeshLoadResult LoadCookedMeshArtifact(const std::filesystem::path& artifactPath)
{
    CookedMeshLoadResult result;
    nlohmann::json document;
    if (!ReadArtifact(artifactPath, "renderer.mesh", document, result))
        return result;

    const auto& payload = document["payload"];
    if (!payload.is_object() || !payload.contains("vertices") || !payload.contains("indices") ||
        !payload["vertices"].is_array() || !payload["indices"].is_array())
    {
        Fail(result, "RenderArtifact.MeshPayloadInvalid", "Mesh payload needs vertices and indices arrays.");
        return result;
    }

    const std::string artifactId = document["artifactId"].get<std::string>();
    MeshLod& lod = result.asset.lods[0];
    MeshVec3 minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max()};
    MeshVec3 maximum{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                     std::numeric_limits<float>::lowest()};
    try
    {
        for (const auto& vertexValue : payload["vertices"])
        {
            if (!vertexValue.is_array() || vertexValue.size() != 3)
            {
                Fail(result, "RenderArtifact.MeshPayloadInvalid", "Mesh vertex must have three numbers.");
                return result;
            }
            const auto x = vertexValue[0].get<double>();
            const auto y = vertexValue[1].get<double>();
            const auto z = vertexValue[2].get<double>();
            const auto finiteFloat = [](double value) {
                return std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max();
            };
            if (!finiteFloat(x) || !finiteFloat(y) || !finiteFloat(z))
            {
                Fail(result, "RenderArtifact.MeshNonFinite", "Mesh vertices must contain finite values.");
                return result;
            }
            MeshVertex vertex{};
            vertex.position = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
            vertex.normal = {0.0f, 1.0f, 0.0f};
            vertex.tangent = {1.0f, 0.0f, 0.0f};
            lod.vertices.push_back(vertex);
            minimum.x = std::min(minimum.x, vertex.position.x);
            minimum.y = std::min(minimum.y, vertex.position.y);
            minimum.z = std::min(minimum.z, vertex.position.z);
            maximum.x = std::max(maximum.x, vertex.position.x);
            maximum.y = std::max(maximum.y, vertex.position.y);
            maximum.z = std::max(maximum.z, vertex.position.z);
        }
        for (const auto& index : payload["indices"])
            lod.indices.push_back(index.get<std::uint32_t>());
    }
    catch (const nlohmann::json::exception& exception)
    {
        Fail(result, "RenderArtifact.MeshPayloadInvalid", exception.what());
        return result;
    }
    if (lod.vertices.empty() || lod.indices.empty() ||
        lod.vertices.size() > kMaxMeshVerticesPerLod || lod.indices.size() > kMaxMeshIndicesPerLod ||
        std::any_of(lod.indices.begin(), lod.indices.end(), [&](std::uint32_t index) {
            return index >= lod.vertices.size();
        }))
    {
        Fail(result, "RenderArtifact.MeshBoundsInvalid", "Mesh payload is empty, over budget, or has invalid indices.");
        return result;
    }

    result.asset.meshId = StableId(artifactId);
    result.asset.debugName = artifactId;
    result.asset.lodCount = 1;
    result.asset.rootBounds.centre = {(minimum.x + maximum.x) * 0.5f,
                                      (minimum.y + maximum.y) * 0.5f,
                                      (minimum.z + maximum.z) * 0.5f};
    result.asset.rootBounds.halfExtents = {(maximum.x - minimum.x) * 0.5f,
                                           (maximum.y - minimum.y) * 0.5f,
                                           (maximum.z - minimum.z) * 0.5f};
    lod.vertexCountHint = static_cast<std::uint32_t>(lod.vertices.size());
    lod.indexCountHint = static_cast<std::uint32_t>(lod.indices.size());
    lod.approxGpuBytes = lod.vertices.size() * sizeof(MeshVertex) + lod.indices.size() * sizeof(std::uint32_t);
    lod.submeshes.push_back({0u, lod.indexCountHint, 0u, artifactId});
    return result;
}

CookedMaterialLoadResult LoadCookedMaterialArtifact(
    const std::filesystem::path& artifactPath,
    ShaderTemplateHandle solidShaderHandle)
{
    CookedMaterialLoadResult result;
    nlohmann::json document;
    if (!ReadArtifact(artifactPath, "renderer.material", document, result))
        return result;

    const auto& payload = document["payload"];
    if (!payload.is_object() ||
        !payload.contains("baseColor") || !payload["baseColor"].is_array() ||
        payload["baseColor"].size() != 4 || solidShaderHandle == ShaderTemplateHandle::kInvalid)
    {
        Fail(result, "RenderArtifact.MaterialPayloadInvalid",
             "Material payload requires renderer.solid, RGBA baseColor, and a resolved shader handle.");
        return result;
    }
    if (payload.value("shader", std::string{}) != "renderer.solid")
    {
        Fail(result, "RenderArtifact.ShaderUnsupported", "Material shader identity must be renderer.solid.");
        return result;
    }
    try
    {
        const std::string artifactId = document["artifactId"].get<std::string>();
        result.material = MakeErrorMaterial();
        result.material.materialId = StableId(artifactId);
        result.material.shaderHandle = solidShaderHandle;
        const auto x = payload["baseColor"][0].get<double>();
        const auto y = payload["baseColor"][1].get<double>();
        const auto z = payload["baseColor"][2].get<double>();
        const auto w = payload["baseColor"][3].get<double>();
        result.material.vectors[0] = {static_cast<float>(x), static_cast<float>(y),
                                      static_cast<float>(z), static_cast<float>(w)};
        const auto& color = result.material.vectors[0];
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(w) ||
            std::abs(x) > std::numeric_limits<float>::max() || std::abs(y) > std::numeric_limits<float>::max() ||
            std::abs(z) > std::numeric_limits<float>::max() || std::abs(w) > std::numeric_limits<float>::max())
        {
            Fail(result, "RenderArtifact.MaterialNonFinite", "Material values must be finite.");
            result.material = MakeErrorMaterial();
        }
    }
    catch (const nlohmann::json::exception& exception)
    {
        Fail(result, "RenderArtifact.MaterialPayloadInvalid", exception.what());
    }
    return result;
}

} // namespace SagaEngine::Render
