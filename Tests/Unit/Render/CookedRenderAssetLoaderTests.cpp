#include <SagaEngine/Render/Assets/CookedRenderAssetLoader.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <array>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{

std::string Sha256(const std::string& value)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    auto* context = EVP_MD_CTX_new();
    EXPECT_NE(context, nullptr);
    EXPECT_EQ(EVP_DigestInit_ex(context, EVP_sha256(), nullptr), 1);
    EXPECT_EQ(EVP_DigestUpdate(context, value.data(), value.size()), 1);
    EXPECT_EQ(EVP_DigestFinal_ex(context, digest.data(), &length), 1);
    EVP_MD_CTX_free(context);
    std::ostringstream output;
    output << "sha256:" << std::hex << std::setfill('0');
    for (unsigned int index = 0; index < length; ++index)
        output << std::setw(2) << static_cast<unsigned int>(digest[index]);
    return output.str();
}

std::filesystem::path WriteArtifact(std::string name, std::string type, nlohmann::json payload)
{
    auto document = nlohmann::json{
        {"schemaVersion", 1}, {"artifactId", "fixture." + name}, {"artifactType", type},
        {"contentHash", Sha256(payload.dump(2) + "\n")},
        {"sourceHash", "sha256:" + std::string(64, '1')}, {"payload", std::move(payload)}};
    const auto path = std::filesystem::temp_directory_path() / ("saga-cooked-" + name + ".json");
    std::ofstream(path) << document.dump(2) << '\n';
    return path;
}

nlohmann::json MeshPayload()
{
    return {{"primitive", "plane"},
            {"vertices", {{-1, 0, -1}, {1, 0, -1}, {1, 0, 1}, {-1, 0, 1}}},
            {"indices", {0, 1, 2, 0, 2, 3}}};
}

TEST(CookedRenderAssetLoader, LoadsValidatedMeshAndMaterial)
{
    const auto meshPath = WriteArtifact("mesh", "renderer.mesh", MeshPayload());
    const auto materialPath = WriteArtifact("material", "renderer.material",
                                             {{"baseColor", {0.2, 0.4, 0.8, 1.0}}, {"shader", "renderer.solid"}});
    EXPECT_TRUE(SagaEngine::Render::LoadCookedMeshArtifact(meshPath).Succeeded());
    EXPECT_TRUE(SagaEngine::Render::LoadCookedMaterialArtifact(materialPath,
        static_cast<SagaEngine::Render::ShaderTemplateHandle>(7u)).Succeeded());
    std::filesystem::remove(meshPath);
    std::filesystem::remove(materialPath);
}

TEST(CookedRenderAssetLoader, RejectsTamperedMalformedAndWrongTypeArtifacts)
{
    const auto path = WriteArtifact("tampered", "renderer.mesh", MeshPayload());
    auto document = nlohmann::json::parse(std::ifstream(path));
    document["payload"]["vertices"][0][0] = -2;
    std::ofstream(path) << document.dump(2) << '\n';
    EXPECT_EQ(SagaEngine::Render::LoadCookedMeshArtifact(path).diagnostics[0].diagnosticId,
              "RenderArtifact.ContentHashMismatch");
    document["contentHash"] = "sha256:fixture";
    std::ofstream(path) << document.dump(2) << '\n';
    EXPECT_EQ(SagaEngine::Render::LoadCookedMeshArtifact(path).diagnostics[0].diagnosticId,
              "RenderArtifact.HashMalformed");
    document["contentHash"] = Sha256(document["payload"].dump(2) + "\n");
    document["sourceHash"] = "sha256:source";
    std::ofstream(path) << document.dump(2) << '\n';
    EXPECT_EQ(SagaEngine::Render::LoadCookedMeshArtifact(path).diagnostics[0].diagnosticId,
              "RenderArtifact.HashMalformed");
    document["sourceHash"] = "sha256:" + std::string(64, '1');
    document["artifactType"] = "renderer.material";
    std::ofstream(path) << document.dump(2) << '\n';
    EXPECT_EQ(SagaEngine::Render::LoadCookedMeshArtifact(path).diagnostics[0].diagnosticId,
              "RenderArtifact.TypeMismatch");
    std::filesystem::remove(path);
}

TEST(CookedRenderAssetLoader, RejectsBoundsNonFiniteAndUnsupportedShader)
{
    auto bounds = MeshPayload();
    bounds["indices"][0] = 99;
    const auto boundsPath = WriteArtifact("bounds", "renderer.mesh", bounds);
    EXPECT_EQ(SagaEngine::Render::LoadCookedMeshArtifact(boundsPath).diagnostics[0].diagnosticId,
              "RenderArtifact.MeshBoundsInvalid");
    auto nonFinite = MeshPayload();
    nonFinite["vertices"][0][0] = 1e100;
    const auto finitePath = WriteArtifact("finite", "renderer.mesh", nonFinite);
    EXPECT_EQ(SagaEngine::Render::LoadCookedMeshArtifact(finitePath).diagnostics[0].diagnosticId,
              "RenderArtifact.MeshNonFinite");
    const auto materialFinitePath = WriteArtifact("material-finite", "renderer.material",
        {{"baseColor", {1e100, 1, 1, 1}}, {"shader", "renderer.solid"}});
    EXPECT_EQ(SagaEngine::Render::LoadCookedMaterialArtifact(materialFinitePath,
        static_cast<SagaEngine::Render::ShaderTemplateHandle>(7u)).diagnostics[0].diagnosticId,
        "RenderArtifact.MaterialNonFinite");
    const auto shaderPath = WriteArtifact("shader", "renderer.material",
        {{"baseColor", {1, 1, 1, 1}}, {"shader", "sample.special"}});
    EXPECT_EQ(SagaEngine::Render::LoadCookedMaterialArtifact(shaderPath,
        static_cast<SagaEngine::Render::ShaderTemplateHandle>(7u)).diagnostics[0].diagnosticId,
        "RenderArtifact.ShaderUnsupported");
    std::filesystem::remove(boundsPath);
    std::filesystem::remove(finitePath);
    std::filesystem::remove(materialFinitePath);
    std::filesystem::remove(shaderPath);
}

TEST(CookedRenderAssetLoader, LoadsSagaPackagerProducedArtifact)
{
    const auto root = std::filesystem::temp_directory_path() / "saga-render-producer-consumer";
    std::filesystem::remove_all(root);
    const auto tool = std::filesystem::path(SAGA_SOURCE_ROOT) / "Tools/SagaPackager/sagapack";
    const auto project = std::filesystem::path(SAGA_SOURCE_ROOT) / "Samples/StarterArena/StarterArena.sagaproj";
    std::ostringstream command;
    command << std::quoted(tool.string()) << " compile-assets --project " << std::quoted(project.string())
            << " --out " << std::quoted(root.string());
    ASSERT_EQ(std::system(command.str().c_str()), 0);
    const auto manifest = nlohmann::json::parse(std::ifstream(root / "manifests/current.json"));
    const auto iterator = std::find_if(manifest["artifacts"].begin(), manifest["artifacts"].end(),
        [](const auto& item) { return item["artifactType"] == "renderer.mesh"; });
    ASSERT_NE(iterator, manifest["artifacts"].end());
    const auto artifact = *iterator;
    EXPECT_TRUE(SagaEngine::Render::LoadCookedMeshArtifact(root / artifact["path"].get<std::string>()).Succeeded());
    std::filesystem::remove_all(root);
}

} // namespace
