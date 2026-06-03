/// @file CompilerSessionTests.cpp
/// @brief Tests for project-level compiler lifecycle contracts.

#include "SDE/Compiler/CompilerSession.h"
#include "SDE/Compiler/CompilerFacade.h"
#include "SDE/Core/Cancellation.h"
#include "SDE/IO/ModelWriter.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace SDE;
namespace fs = std::filesystem;

namespace
{

fs::path MakeProjectDir(const std::string& name)
{
    fs::path root = fs::temp_directory_path() / name;
    fs::remove_all(root);
    fs::create_directories(root / "schemas");
    fs::create_directories(root / "data");
    return root;
}

fs::path MakeNativeProjectDir(const std::string& name)
{
    fs::path root = fs::temp_directory_path() / name;
    fs::remove_all(root);
    fs::create_directories(root / "source");
    return root;
}

void WriteFile(const fs::path& path, const std::string& text)
{
    std::ofstream out(path);
    out << text;
}

std::string ReadFile(const fs::path& path)
{
    std::ifstream in(path);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

CompileRequest MakeCompileRequest(const fs::path& root)
{
    CompileRequest request;
    request.workspaceRoot = root;
    request.outputRoot = root / "artifacts";
    request.domain = "Definition.Contract";
    request.artifactKind = "DefinitionGraph";
    return request;
}

} // namespace

TEST(CompilerSessionTest, CancelBeforeLoadPublishesNoGraph)
{
    fs::path root = MakeProjectDir("sde_cancelled_project");
    CancellationSource source;
    source.RequestCancellation();

    CompilerSession session({root});
    CompileContext context;
    context.cancellation = source.Token();

    auto result = session.Compile(context);

    EXPECT_TRUE(result.cancelled);
    EXPECT_EQ(result.state, CompileState::Aborted);
    EXPECT_FALSE(result.graph.has_value());
    ASSERT_EQ(result.validation.diagnostics.size(), 1u);
    EXPECT_EQ(result.validation.diagnostics[0].code, "SDE_CANCELLED");
}

TEST(CompilerSessionTest, ImportCycleIsRejected)
{
    fs::path root = MakeProjectDir("sde_import_cycle_project");
    WriteFile(root / "schemas" / "a.json", R"({
        "id": "A",
        "schemaVersion": 1,
        "imports": [ "B" ],
        "fields": [ { "id": "name", "type": "Text" } ]
    })");
    WriteFile(root / "schemas" / "b.json", R"({
        "id": "B",
        "schemaVersion": 1,
        "imports": [ "A" ],
        "fields": [ { "id": "name", "type": "Text" } ]
    })");

    CompilerSession session({root});
    auto result = session.Compile();

    EXPECT_EQ(result.state, CompileState::ValidationFailed);
    ASSERT_FALSE(result.validation.diagnostics.empty());
    EXPECT_EQ(result.validation.diagnostics[0].code, "SDE_IMPORT_CYCLE");
    EXPECT_FALSE(result.graph.has_value());
}

TEST(CompilerSessionTest, NativeSdeSourceCompilesWithLanguageAndModelVersions)
{
    fs::path root = MakeNativeProjectDir("sde_native_source_project");
    WriteFile(root / "source" / "profiles.sde", R"(sde version 1

package Saga.Editor.Customization

model EditorProfile version 1 {
  field displayName Text required
  field defaultPanels List<Text> optional
  field showStatusBar Boolean optional
}

instance EditorProfile saga.profile.project {
  displayName = "Project"
  defaultPanels = ["Hierarchy", "Inspector"]
  showStatusBar = true
}
)");

    CompilerSession session({root});
    auto result = session.Compile();

    ASSERT_TRUE(IsUsable(result.state));
    ASSERT_TRUE(result.graph.has_value());
    EXPECT_EQ(result.versions.language.major, 1u);
    EXPECT_FALSE(result.hashes.sourceHash.empty());
    EXPECT_FALSE(result.hashes.artifactHash.empty());
    EXPECT_TRUE(result.graph->Find("EditorProfile", "saga.profile.project") != nullptr);
}

TEST(CompilerSessionTest, NativeSdeSourceRequiresLanguageVersion)
{
    fs::path root = MakeNativeProjectDir("sde_native_source_missing_version");
    WriteFile(root / "source" / "broken.sde", R"(package Saga.Editor.Customization)");

    CompilerSession session({root});
    auto result = session.Compile();

    EXPECT_EQ(result.state, CompileState::ValidationFailed);
    ASSERT_FALSE(result.validation.diagnostics.empty());
    EXPECT_EQ(result.validation.diagnostics.front().code, "SDE_EXPECTED_KEYWORD");
}

TEST(CompilerSessionTest, NativeSdeSourceRejectsDuplicateInstances)
{
    fs::path root = MakeNativeProjectDir("sde_native_source_duplicate_instances");
    WriteFile(root / "source" / "profiles.sde", R"(sde version 1

package Saga.Editor.Customization

model EditorProfile version 1 {
  field displayName Text required
}

instance EditorProfile saga.profile.project {
  displayName = "One"
}

instance EditorProfile saga.profile.project {
  displayName = "Two"
}
)");

    CompilerSession session({root});
    auto result = session.Compile();

    EXPECT_EQ(result.state, CompileState::ValidationFailed);
    bool found = false;
    for (const auto& diagnostic : result.validation.diagnostics)
        found = found || diagnostic.code == "SDE_DUPLICATE_INSTANCE";
    EXPECT_TRUE(found);
}

TEST(CompilerSessionTest, CompilePublishesStableArtifactManifestAndGraphOutput)
{
    constexpr const char* source = R"(sde version 1

package Definition.Contract

model ContractItem version 1 {
  field displayName Text required
  field power Integer required
}

instance ContractItem item.alpha {
  displayName = "Alpha"
  power = 7
}
)";

    fs::path rootA = MakeNativeProjectDir("sde_artifact_contract_a");
    fs::path rootB = MakeNativeProjectDir("sde_artifact_contract_b");
    WriteFile(rootA / "source" / "contract.sde", source);
    WriteFile(rootB / "source" / "contract.sde", source);

    CompilerFacade compiler;
    auto resultA = compiler.Compile(MakeCompileRequest(rootA));
    auto resultB = compiler.Compile(MakeCompileRequest(rootB));

    ASSERT_TRUE(IsUsable(resultA.project.state));
    ASSERT_TRUE(IsUsable(resultB.project.state));
    ASSERT_TRUE(resultA.project.graph.has_value());
    ASSERT_TRUE(resultB.project.graph.has_value());
    EXPECT_TRUE(resultA.project.validation.diagnostics.empty());
    EXPECT_TRUE(resultB.project.validation.diagnostics.empty());

    EXPECT_EQ(resultA.manifest.artifactVersion.ToString(), resultB.manifest.artifactVersion.ToString());
    EXPECT_EQ(resultA.manifest.compilerVersion.ToString(), resultB.manifest.compilerVersion.ToString());
    EXPECT_EQ(resultA.manifest.languageVersion, resultB.manifest.languageVersion);
    EXPECT_EQ(resultA.manifest.domain, "Definition.Contract");
    EXPECT_EQ(resultA.manifest.kind, "DefinitionGraph");
    EXPECT_EQ(resultA.manifest.domain, resultB.manifest.domain);
    EXPECT_EQ(resultA.manifest.kind, resultB.manifest.kind);
    EXPECT_EQ(resultA.manifest.sourceHash, resultB.manifest.sourceHash);
    EXPECT_EQ(resultA.manifest.dependencyHash, resultB.manifest.dependencyHash);
    EXPECT_EQ(resultA.manifest.modelHashes, resultB.manifest.modelHashes);

    ASSERT_EQ(resultA.manifest.outputs.size(), 3u);
    ASSERT_EQ(resultB.manifest.outputs.size(), 3u);
    EXPECT_EQ(resultA.manifest.outputs[0].kind, "CompiledGraph");
    EXPECT_EQ(resultA.manifest.outputs[0].path, "graph.json");
    EXPECT_EQ(resultA.manifest.outputs[1].kind, "Diagnostics");
    EXPECT_EQ(resultA.manifest.outputs[1].path, "diagnostics.json");
    EXPECT_EQ(resultA.manifest.outputs[2].kind, "Hashes");
    EXPECT_EQ(resultA.manifest.outputs[2].path, "hashes.json");
    for (std::size_t i = 0; i < resultA.manifest.outputs.size(); ++i)
    {
        EXPECT_EQ(resultA.manifest.outputs[i].kind, resultB.manifest.outputs[i].kind);
        EXPECT_EQ(resultA.manifest.outputs[i].path, resultB.manifest.outputs[i].path);
        EXPECT_EQ(resultA.manifest.outputs[i].hash, resultB.manifest.outputs[i].hash);
    }

    EXPECT_FALSE(resultA.project.hashes.sourceHash.empty());
    EXPECT_FALSE(resultA.project.hashes.dependencyHash.empty());
    EXPECT_FALSE(resultA.project.hashes.compiledGraphHash.empty());
    EXPECT_FALSE(resultA.project.hashes.artifactHash.empty());
    EXPECT_EQ(resultA.project.hashes.sourceHash, resultB.project.hashes.sourceHash);
    EXPECT_EQ(resultA.project.hashes.schemaHash, resultB.project.hashes.schemaHash);
    EXPECT_EQ(resultA.project.hashes.dataHash, resultB.project.hashes.dataHash);
    EXPECT_EQ(resultA.project.hashes.dependencyHash, resultB.project.hashes.dependencyHash);
    EXPECT_EQ(resultA.project.hashes.compiledGraphHash, resultB.project.hashes.compiledGraphHash);
    EXPECT_EQ(resultA.project.hashes.artifactHash, resultB.project.hashes.artifactHash);
    EXPECT_EQ(resultA.project.hashes.modelHashes, resultB.project.hashes.modelHashes);

    fs::create_directories(rootA / "artifacts");
    fs::create_directories(rootB / "artifacts");
    JsonModelWriter writer;
    std::string writeError;
    ASSERT_TRUE(writer.Write(*resultA.project.graph, (rootA / "artifacts" / "graph.json").string(), &writeError))
        << writeError;
    ASSERT_TRUE(writer.Write(*resultB.project.graph, (rootB / "artifacts" / "graph.json").string(), &writeError))
        << writeError;
    EXPECT_EQ(ReadFile(rootA / "artifacts" / "graph.json"),
              ReadFile(rootB / "artifacts" / "graph.json"));
}

TEST(CompilerSessionTest, InvalidSourceDiagnosticsRemainStableAcrossCompiles)
{
    constexpr const char* source = R"(sde version 1

package Definition.Contract

model ContractItem version 1 {
  field displayName Text required
}

instance ContractItem item.duplicate {
  displayName = "First"
}

instance ContractItem item.duplicate {
  displayName = "Second"
}
)";

    fs::path rootA = MakeNativeProjectDir("sde_artifact_invalid_a");
    fs::path rootB = MakeNativeProjectDir("sde_artifact_invalid_b");
    WriteFile(rootA / "source" / "contract.sde", source);
    WriteFile(rootB / "source" / "contract.sde", source);

    CompilerFacade compiler;
    auto resultA = compiler.Compile(MakeCompileRequest(rootA));
    auto resultB = compiler.Compile(MakeCompileRequest(rootB));

    EXPECT_EQ(resultA.project.state, CompileState::ValidationFailed);
    EXPECT_EQ(resultB.project.state, CompileState::ValidationFailed);
    EXPECT_FALSE(resultA.project.graph.has_value());
    EXPECT_FALSE(resultB.project.graph.has_value());
    ASSERT_EQ(resultA.project.validation.diagnostics.size(), 1u);
    ASSERT_EQ(resultB.project.validation.diagnostics.size(), 1u);

    const Diagnostic& diagA = resultA.project.validation.diagnostics.front();
    const Diagnostic& diagB = resultB.project.validation.diagnostics.front();
    EXPECT_EQ(diagA.severity, diagB.severity);
    EXPECT_EQ(diagA.category, diagB.category);
    EXPECT_EQ(diagA.code, "SDE_DUPLICATE_INSTANCE");
    EXPECT_EQ(diagA.code, diagB.code);
    EXPECT_EQ(diagA.message, diagB.message);
    EXPECT_EQ(diagA.metadata, diagB.metadata);
}
