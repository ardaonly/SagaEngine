/// @file SagaProductTests.cpp
/// @brief Tests for Saga product orchestration startup boundaries.

#include "SagaAppConfig.h"
#include "SagaProjectSystem.h"
#include "SagaProductHost.h"
#include "SagaSdeCompiler.h"
#include "SagaWorkspaceResolver.h"

#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Host/EditorWorkspaceDefinition.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#ifndef SAGA_SOURCE_ROOT
#define SAGA_SOURCE_ROOT "."
#endif

namespace
{

namespace fs = std::filesystem;
using namespace SagaProduct;

[[nodiscard]] fs::path SourceRoot()
{
    return fs::path(SAGA_SOURCE_ROOT);
}

[[nodiscard]] fs::path BuiltInBasicWorkspaceRoot()
{
    return SourceRoot() / "Apps" / "Saga" / "Definitions" / "BasicWorkspace";
}

[[nodiscard]] fs::path MakeTempDir(const std::string& name)
{
    fs::path root = fs::temp_directory_path() / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void WriteFile(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path);
    out << text;
}

TEST(SagaAppConfigTest, DefaultConfigCanBeCreated)
{
    const char* argvRaw[] = { "Saga" };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(1, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.config.workspaceSelector, "builtin:basic");
    EXPECT_EQ(result.config.target, SagaProductTargetKind::Editor);
    EXPECT_FALSE(result.config.prepareOnly);
}

TEST(SagaAppConfigTest, QtLaunchArgumentsArePassedThrough)
{
    const char* argvRaw[] = {
        "Saga",
        "--platform",
        "offscreen",
        "--style=Fusion",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(4, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.config.target, SagaProductTargetKind::Editor);
    EXPECT_FALSE(result.config.prepareOnly);
}

TEST(SagaProductInfoTest, VersionInfoResolvesFromDistributionJson)
{
    const fs::path root = MakeTempDir("saga_product_info_test");
    const fs::path versionJson = root / "version.json";
    WriteFile(versionJson, R"({
        "productName": "SAGA",
        "distributionVersion": "0.0.8-test",
        "buildVersion": "0.0.8",
        "gitCommit": "abc123",
        "platform": "Linux",
        "binaries": [ "Saga" ]
    })");

    SagaAppConfig config;
    config.versionInfoPath = versionJson;

    std::string error;
    const SagaProductInfo info = LoadProductInfo(config, error);

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(info.productName, "SAGA");
    EXPECT_EQ(info.distributionVersion, "0.0.8-test");
    EXPECT_EQ(info.buildVersion, "0.0.8");
    EXPECT_EQ(info.gitCommit, "abc123");
    EXPECT_EQ(info.binaries.size(), 1u);
    EXPECT_EQ(info.binaries.front(), "Saga");
}

TEST(SagaWorkspaceResolverTest, BuiltInBasicWorkspaceResolvesThroughSde)
{
    SagaWorkspaceResolver resolver;
    SagaWorkspaceResolveRequest request;
    request.selector = "builtin:basic";
    request.builtInBasicRoot = BuiltInBasicWorkspaceRoot();

    const SagaWorkspaceResolveResult result = resolver.Resolve(request);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.workspace.id, "builtin.basic");
    EXPECT_EQ(result.workspace.editorProfile, "saga.profile.basic");
    EXPECT_EQ(result.workspace.runtimeRole, "SagaRuntime");
    EXPECT_EQ(result.workspace.serverRole, "SagaServer");
    EXPECT_FALSE(result.workspace.artifactHash.empty());
}

TEST(SagaWorkspaceResolverTest, MissingSdeFailsDeterministically)
{
    SagaWorkspaceResolver resolver;
    SagaWorkspaceResolveRequest request;
    request.selector = (MakeTempDir("saga_missing_sde_test") / "missing").string();
    request.builtInBasicRoot = BuiltInBasicWorkspaceRoot();

    const SagaWorkspaceResolveResult result = resolver.Resolve(request);

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("missing"), std::string::npos);
}

TEST(SagaWorkspaceResolverTest, InvalidSdeFailsDeterministically)
{
    const fs::path root = MakeTempDir("saga_invalid_sde_test");
    WriteFile(root / "source" / "workspace.sde", "sde version nope");

    SagaWorkspaceResolver resolver;
    SagaWorkspaceResolveRequest request;
    request.selector = root.string();
    request.builtInBasicRoot = BuiltInBasicWorkspaceRoot();

    const SagaWorkspaceResolveResult result = resolver.Resolve(request);

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("validation failed"), std::string::npos);
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_TRUE(std::any_of(result.diagnostics.begin(),
                            result.diagnostics.end(),
                            [](const std::string& diagnostic)
                            {
                                return diagnostic.find("SDE_EXPECTED_TOKEN") !=
                                       std::string::npos;
                            }));
}

TEST(SagaProductHostTest, PreparesRoleTargetsAtBoundaryLevel)
{
    SagaWorkspaceDefinition workspace;
    workspace.id = "builtin.basic";
    workspace.root = BuiltInBasicWorkspaceRoot();
    workspace.editorProfile = "saga.profile.basic";
    workspace.runtimeRole = "SagaRuntime";
    workspace.serverRole = "SagaServer";

    SagaProductHost host;
    SagaSessionModel session;
    session.workspace = workspace;

    session.target = SagaProductTargetKind::Editor;
    SagaPreparedTarget editor = host.PrepareTarget(session);
    EXPECT_EQ(editor.executableName, "Saga");
    EXPECT_EQ(editor.moduleName, "SagaEditorModule");
    EXPECT_TRUE(editor.sameProcess);
    EXPECT_TRUE(editor.arguments.empty());

    session.target = SagaProductTargetKind::Runtime;
    SagaPreparedTarget runtime = host.PrepareTarget(session);
    EXPECT_EQ(runtime.executableName, "Saga");
    EXPECT_EQ(runtime.moduleName, "SagaRuntimeModule");

    session.target = SagaProductTargetKind::Server;
    SagaPreparedTarget server = host.PrepareTarget(session);
    EXPECT_EQ(server.executableName, "Saga");
    EXPECT_EQ(server.moduleName, "SagaServerModule");
}

TEST(SagaProjectSystemTest, CreateOpenAndRememberProjectAreReal)
{
    const fs::path root = MakeTempDir("saga_project_system_test");
    SagaProjectSystem projects(root / "recent_projects.json");

    const SagaProjectResult created =
        projects.CreateProject(root, "FirstProject");

    ASSERT_TRUE(created.ok) << created.error;
    EXPECT_TRUE(fs::exists(created.manifest.root / "saga.project.json"));
    EXPECT_TRUE(fs::exists(created.manifest.root / ".sde" / "source" / "workspace.sde"));
    EXPECT_TRUE(fs::exists(created.manifest.root / ".sde" / "source" / "editor" / "profiles.sde"));
    EXPECT_TRUE(fs::exists(created.manifest.root / ".sde" / "artifacts"));
    EXPECT_TRUE(fs::exists(created.manifest.root / ".sde" / "cache"));
    EXPECT_TRUE(fs::exists(created.manifest.root / "Assets"));
    EXPECT_EQ(created.manifest.sdeRoot, created.manifest.root / ".sde");

    const SagaProjectResult opened = projects.OpenProject(created.manifest.root);
    ASSERT_TRUE(opened.ok) << opened.error;
    EXPECT_EQ(opened.manifest.displayName, "FirstProject");
    EXPECT_EQ(opened.manifest.sdeRoot, opened.manifest.root / ".sde");

    const std::vector<SagaRecentProject> recent = projects.LoadRecentProjects();
    ASSERT_FALSE(recent.empty());
    EXPECT_EQ(recent.front().displayName, "FirstProject");
    EXPECT_TRUE(recent.front().exists);
}

TEST(SagaProjectSystemTest, InvalidRoomCodesFailWithoutFakeJoin)
{
    EXPECT_TRUE(SagaProjectSystem::ValidateRoomCode("").has_value());
    EXPECT_TRUE(SagaProjectSystem::ValidateRoomCode("BAD").has_value());
    EXPECT_TRUE(SagaProjectSystem::ValidateRoomCode("ROOM 1234").has_value());
    EXPECT_FALSE(SagaProjectSystem::ValidateRoomCode("ROOM-1234").has_value());
}

TEST(SagaSdeCompilerTest, CompileUsesRealSdeAndDoesNotReplaceLastGoodOnFailure)
{
    const fs::path root = MakeTempDir("saga_sde_compiler_test");
    SagaProjectSystem projects(root / "recent_projects.json");
    const SagaProjectResult created = projects.CreateProject(root, "CompileProject");
    ASSERT_TRUE(created.ok) << created.error;

    SagaSdeCompiler compiler;
    const SagaSdeCompileResult good =
        compiler.Compile(created.manifest.sdeRoot);

    ASSERT_TRUE(good.sdeAvailable);
    ASSERT_TRUE(good.ok) << good.message;
    ASSERT_TRUE(good.hashes.has_value());
    EXPECT_FALSE(good.hashes->artifactHash.empty());
    ASSERT_TRUE(compiler.LastGoodHashes().has_value());

    WriteFile(created.manifest.sdeRoot / "source" / "Broken.sde", "sde version nope");
    const SagaSdeCompileResult bad =
        compiler.Compile(created.manifest.sdeRoot);

    EXPECT_FALSE(bad.ok);
    EXPECT_TRUE(compiler.LastGoodHashes().has_value());
    EXPECT_EQ(compiler.LastGoodHashes()->artifactHash, good.hashes->artifactHash);
}

TEST(SagaEditorBoundaryTest, EditorConsumesPreparedWorkspaceWithoutProductOrchestration)
{
    SagaEditor::EditorWorkspaceDefinition workspace;
    workspace.id = "builtin.basic";
    workspace.root = BuiltInBasicWorkspaceRoot();
    workspace.initialProfileId = "saga.profile.basic";
    workspace.sdeValidated = true;

    SagaEditor::EditorHost host;
    ASSERT_TRUE(host.Init(
        std::make_unique<SagaEditor::MemoryEditorSettingsStore>(),
        workspace));

    ASSERT_TRUE(host.GetWorkspaceDefinition().has_value());
    EXPECT_EQ(host.GetWorkspaceDefinition()->id, "builtin.basic");
    EXPECT_TRUE(host.GetWorkspaceDefinition()->sdeValidated);

    const fs::path editorMain = SourceRoot() / "Apps" / "Editor" / "main.cpp";
    std::ifstream input(editorMain);
    ASSERT_TRUE(input.is_open());
    const std::string text((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());

    EXPECT_EQ(text.find("--host"), std::string::npos);
    EXPECT_EQ(text.find("--join"), std::string::npos);
    EXPECT_EQ(text.find("--server"), std::string::npos);
    EXPECT_EQ(text.find("SagaProduct"), std::string::npos);

    host.Shutdown();
}

} // namespace
