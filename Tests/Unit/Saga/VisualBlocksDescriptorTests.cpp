/// @file VisualBlocksDescriptorTests.cpp
/// @brief Evidence-only generation tests for mandatory profile six.

#include "ProductIntegration/VisualBlocksDescriptor.h"

#include <QCryptographicHash>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>

namespace
{

namespace fs = std::filesystem;
using namespace SagaProduct;
using Json = nlohmann::json;

[[nodiscard]] fs::path SourceRoot() { return fs::path(SAGA_SOURCE_ROOT); }

void WriteJson(const fs::path& path, const Json& value)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path);
    output << value.dump(2) << '\n';
}

[[nodiscard]] Json ReadJson(const fs::path& path)
{
    std::ifstream input(path);
    Json value;
    input >> value;
    return value;
}

[[nodiscard]] std::string FileHash(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    const std::string material((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
    return QCryptographicHash::hash(
        QByteArray(material.data(), static_cast<qsizetype>(material.size())),
        QCryptographicHash::Sha256).toHex().toStdString();
}

[[nodiscard]] std::set<std::string> Tree(const fs::path& root)
{
    std::set<std::string> paths;
    for (const auto& entry : fs::recursive_directory_iterator(root))
        paths.insert(fs::relative(entry.path(), root).generic_string());
    return paths;
}

struct Fixture
{
    fs::path root;
    fs::path project;
    fs::path workspace;
    fs::path bindings;
    fs::path artifacts;
    fs::path lifecycle;
    fs::path gameplay;
    fs::path report;

    explicit Fixture(const char* name)
    {
        root = fs::temp_directory_path() / name;
        fs::remove_all(root);
        project = root / "project" / "StarterArena.sagaproj";
        workspace = root / "generated" / "workspace";
        bindings = workspace / "Build" / "Manifests" / "script_bindings.json";
        artifacts = workspace / "Build" / "Manifests" / "script_artifacts.json";
        lifecycle = root / "generated" / "profiles" /
            "starter-arena-lifecycle" / "report.json";
        gameplay = root / "generated" / "profiles" /
            "starter-arena-gameplay" / "report.json";
        report = root / "generated" / "profiles" /
            kVisualBlocksDescriptorProfileId / "report.json";

        WriteJson(project, {{"projectId", "starter-arena"}});
        fs::create_directories(project.parent_path() / "Scripts");
        fs::copy_file(SourceRoot() / "Samples" / "StarterArena" / "Scripts" /
            "GameRules.cs", project.parent_path() / "Scripts" / "GameRules.cs");
        fs::create_directories(workspace / "Scripts");
        fs::copy_file(project.parent_path() / "Scripts" / "GameRules.cs",
                      workspace / "Scripts" / "GameRules.cs");
        const std::string hash = FileHash(project.parent_path() / "Scripts" /
                                          "GameRules.cs");
        WriteJson(bindings, {
            {"sourceHash", "generated-source-set-hash"},
            {"diagnosticSummary", {
                {"errorCount", 0}, {"hasBlockingDiagnostics", false}}},
            {"bindings", {{
                {"bindingId", "script://starter-arena/game-rules#StarterArena.Scripts.GameRules.AddPickupScore"},
                {"scriptId", "script://starter-arena/game-rules"},
                {"declaringType", "StarterArena.Scripts.GameRules"},
                {"methodName", "AddPickupScore"},
                {"parameters", {
                    {{"name", "currentScore"}, {"type", "int"}, {"supported", true}},
                    {{"name", "pickupValue"}, {"type", "int"}, {"supported", true}}}},
                {"returnType", {{"type", "int"}, {"supported", true}}},
                {"authority", "SharedPure"},
                {"sourceFile", (workspace / "Scripts" / "GameRules.cs").string()},
                {"sourceHash", hash}
            }}}
        });
        WriteJson(artifacts, {
            {"sourceHash", "generated-source-set-hash"},
            {"diagnosticSummary", {
                {"errorCount", 0}, {"hasBlockingDiagnostics", false}}},
            {"artifacts", {{
                {"scriptId", "script://starter-arena/game-rules"},
                {"assemblyPath", "Build/Artifacts/Scripts/StarterArenaScripts.scripts.dll"},
                {"runtimeConfigPath", "Build/Artifacts/Scripts/StarterArenaScripts.scripts.runtimeconfig.json"},
                {"bindingIds", {"script://starter-arena/game-rules#StarterArena.Scripts.GameRules.AddPickupScore"}},
                {"sourceFiles", {"Scripts/GameRules.cs"}}
            }}}
        });
        fs::create_directories(workspace / "Build" / "Artifacts" / "Scripts");
        std::ofstream(workspace / "Build" / "Artifacts" / "Scripts" /
            "StarterArenaScripts.scripts.dll") << "fixture";
        WriteJson(workspace / "Build" / "Artifacts" / "Scripts" /
            "StarterArenaScripts.scripts.runtimeconfig.json", Json::object());
        WriteJson(lifecycle, {
            {"status", "Passed"},
            {"scriptLifecycle", {
                {"status", "Passed"},
                {"callbacksObserved", {
                    "OnCreate", "OnStart", "OnUpdate", "OnDestroy"}}}}
        });
        WriteJson(gameplay, {
            {"status", "Passed"},
            {"gameplay", {
                {"status", "Passed"},
                {"capability", "Sample.StarterArena.GameplayState"},
                {"mutations", {
                    {{"operation", "SetBool"}, {"target", "starter.pickup.0.collected"}, {"before", false}, {"after", true}},
                    {{"operation", "AddInt32"}, {"target", "starter.score"}, {"before", 0}, {"after", 10}},
                    {{"operation", "SetString"}, {"target", "starter.player.state"}, {"before", "normal"}, {"after", "powered"}}
                }}}}
        });
        EXPECT_FALSE(fs::exists(project.parent_path() / "VisualBlocks"));
    }

    [[nodiscard]] VisualBlocksDescriptorGenerationRequest Request() const
    {
        VisualBlocksDescriptorGenerationRequest request;
        request.originalProjectManifest = project;
        request.generatedWorkspace = workspace;
        request.bindingManifestPath = bindings;
        request.artifactManifestPath = artifacts;
        request.lifecycleEvidence = {EvidenceStatus::Passed, lifecycle};
        request.gameplayEvidence = {EvidenceStatus::Passed, gameplay};
        request.reportPath = report;
        return request;
    }
};

[[nodiscard]] bool HasCode(
    const VisualBlocksDescriptorGenerationResult& result,
    const std::string& code)
{
    return std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
        [&](const FirstPlayableDiagnostic& diagnostic) {
            return diagnostic.code == code;
        });
}

TEST(VisualBlocksDescriptorGenerationTest, GeneratesCanonicalCatalogFromEvidence)
{
    Fixture fixture("saga_visual_blocks_generation_valid");
    const auto before = Tree(fixture.project.parent_path());
    const auto result = GenerateVisualBlocksDescriptorReport(fixture.Request());

    ASSERT_EQ(result.status, EvidenceStatus::Passed);
    ASSERT_TRUE(result.descriptor.has_value());
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_EQ(result.descriptor->blocks.size(), 12u);
    EXPECT_EQ(result.generatedBlockIds.size(), 12u);
    EXPECT_EQ(result.observedCallbacks.size(), 4u);
    EXPECT_EQ(result.matchedGameplayMutations.size(), 3u);
    const auto& pure = result.descriptor->blocks[4];
    ASSERT_EQ(pure.inputs.size(), 2u);
    EXPECT_EQ(pure.inputs[0].name, "currentScore");
    EXPECT_EQ(pure.inputs[0].type, "int32");
    EXPECT_EQ(pure.inputs[1].name, "pickupValue");
    EXPECT_EQ(pure.outputs[0].type, "int32");
    EXPECT_EQ(result.descriptor->blocks[5].kind, "StateRead");
    EXPECT_EQ(result.descriptor->blocks[7].stateAccess->operation, "SetBool");
    EXPECT_EQ(result.descriptor->blocks[10].kind, "Branch");
    EXPECT_EQ(result.descriptor->blocks[11].kind, "Return");

    const Json report = ReadJson(result.reportPath);
    EXPECT_EQ(report.value("sourceOfTruth", ""), "CSharpAndGeneratedEvidence");
    EXPECT_EQ(report.value("generation", ""), "InProcessReadOnlyGeneration");
    EXPECT_EQ(report["blocks"].size(), 12u);
    EXPECT_EQ(report["nonClaims"].size(), 8u);
    EXPECT_FALSE(report.value("processLaunched", true));
    EXPECT_FALSE(report.value("csharpExecuted", true));
    EXPECT_EQ(Tree(fixture.project.parent_path()), before);
    EXPECT_FALSE(fs::exists(fixture.project.parent_path() / "VisualBlocks"));
}

TEST(VisualBlocksDescriptorGenerationTest, RejectsCallableSignatureDrift)
{
    Fixture fixture("saga_visual_blocks_signature_drift");
    Json bindings = ReadJson(fixture.bindings);
    bindings["bindings"][0]["parameters"][1]["type"] = "long";
    WriteJson(fixture.bindings, bindings);
    const auto result = GenerateVisualBlocksDescriptorReport(fixture.Request());
    EXPECT_EQ(result.status, EvidenceStatus::Failed);
    EXPECT_FALSE(result.descriptor.has_value());
    EXPECT_TRUE(HasCode(result, "ProductShell.VisualBlocks.MethodSignatureMismatch"));
}

TEST(VisualBlocksDescriptorGenerationTest, RejectsSourceAndHashMismatch)
{
    Fixture fixture("saga_visual_blocks_source_hash_drift");
    std::ofstream(fixture.workspace / "Scripts" / "GameRules.cs",
                  std::ios::app) << "// drift\n";
    auto result = GenerateVisualBlocksDescriptorReport(fixture.Request());
    EXPECT_TRUE(HasCode(result, "ProductShell.VisualBlocks.SourceHashMismatch"));

    Fixture escaped("saga_visual_blocks_source_escape");
    Json bindings = ReadJson(escaped.bindings);
    bindings["bindings"][0]["sourceFile"] = (escaped.root / "outside.cs").string();
    std::ofstream(escaped.root / "outside.cs") << "outside";
    WriteJson(escaped.bindings, bindings);
    result = GenerateVisualBlocksDescriptorReport(escaped.Request());
    EXPECT_TRUE(HasCode(result, "ProductShell.VisualBlocks.SourceHashMismatch"));
}

TEST(VisualBlocksDescriptorGenerationTest, RejectsBlockingDiagnostics)
{
    Fixture fixture("saga_visual_blocks_blocking_diagnostics");
    Json bindings = ReadJson(fixture.bindings);
    bindings["diagnosticSummary"]["hasBlockingDiagnostics"] = true;
    WriteJson(fixture.bindings, bindings);
    const auto result = GenerateVisualBlocksDescriptorReport(fixture.Request());
    EXPECT_TRUE(HasCode(result, "ProductShell.VisualBlocks.MetadataMissing"));
}

TEST(VisualBlocksDescriptorGenerationTest, RejectsMissingAndEscapedArtifacts)
{
    Fixture fixture("saga_visual_blocks_artifact_escape");
    Json artifacts = ReadJson(fixture.artifacts);
    artifacts["artifacts"][0]["assemblyPath"] = "../../outside.dll";
    WriteJson(fixture.artifacts, artifacts);
    auto result = GenerateVisualBlocksDescriptorReport(fixture.Request());
    EXPECT_TRUE(HasCode(result, "ProductShell.VisualBlocks.ArtifactMismatch"));

    Fixture missing("saga_visual_blocks_artifact_missing");
    fs::remove(missing.workspace / "Build" / "Artifacts" / "Scripts" /
               "StarterArenaScripts.scripts.dll");
    result = GenerateVisualBlocksDescriptorReport(missing.Request());
    EXPECT_TRUE(HasCode(result, "ProductShell.VisualBlocks.ArtifactMismatch"));
}

TEST(VisualBlocksDescriptorGenerationTest, IncompleteLifecycleDoesNotGenerate)
{
    Fixture fixture("saga_visual_blocks_incomplete_lifecycle");
    Json lifecycle = ReadJson(fixture.lifecycle);
    lifecycle["scriptLifecycle"]["callbacksObserved"].erase(3);
    WriteJson(fixture.lifecycle, lifecycle);
    const auto result = GenerateVisualBlocksDescriptorReport(fixture.Request());
    EXPECT_EQ(result.status, EvidenceStatus::Incomplete);
    EXPECT_FALSE(result.descriptor.has_value());
    EXPECT_TRUE(HasCode(result, "ProductShell.VisualBlocks.RuntimeEvidenceMismatch"));
}

TEST(VisualBlocksDescriptorGenerationTest, RejectsCapabilityAndMutationMismatch)
{
    Fixture fixture("saga_visual_blocks_capability_mismatch");
    Json gameplay = ReadJson(fixture.gameplay);
    gameplay["gameplay"]["capability"] = "Unknown";
    WriteJson(fixture.gameplay, gameplay);
    auto result = GenerateVisualBlocksDescriptorReport(fixture.Request());
    EXPECT_TRUE(HasCode(result, "ProductShell.VisualBlocks.CapabilityMissing"));

    Fixture mutation("saga_visual_blocks_mutation_mismatch");
    gameplay = ReadJson(mutation.gameplay);
    gameplay["gameplay"]["mutations"][1]["after"] = "10";
    WriteJson(mutation.gameplay, gameplay);
    result = GenerateVisualBlocksDescriptorReport(mutation.Request());
    EXPECT_TRUE(HasCode(result, "ProductShell.VisualBlocks.RuntimeEvidenceMismatch"));
}

TEST(VisualBlocksDescriptorBoundaryTest, GeneratorHasNoExecutionOrPrivateDependencies)
{
    std::ifstream input(SourceRoot() /
        "Engine/Source/Editor/VisualBlocksEditor/Private/ProductIntegration/VisualBlocksDescriptor.cpp");
    const std::string source((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
    for (const char* forbidden : {"QProcess", "EvidenceProcessRunner",
        "SagaEngine/Networking", "SagaEngine/ECS", "void*",
        "uintptr_t", "ScriptHandle"})
        EXPECT_EQ(source.find(forbidden), std::string::npos) << forbidden;
}

} // namespace
