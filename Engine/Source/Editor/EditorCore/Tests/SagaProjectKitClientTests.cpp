// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Project/SagaProjectKitClient.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace
{
namespace fs = std::filesystem;

fs::path TempRoot(std::string_view name)
{
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() /
        ("saga-project-kit-client-" + std::string(name) + "-" +
         std::to_string(stamp));
    fs::create_directories(root);
    return root;
}

void Touch(const fs::path& path)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << "fixture\n";
}
}

TEST(SagaProjectKitClientTests, ResolvesInstalledProductFromExecutableNotCwd)
{
    const auto root = TempRoot("installed");
    Touch(root / "bin/Saga");
    Touch(root / "bin/sagaproject");
    Touch(root / "bin/sagapack");
    Touch(root / "bin/sagascript");
    Touch(root / "samples/StarterArena/StarterArena.sagaproj");
    fs::create_directories(root / "Engine/Distribution");
    Touch(root / "Engine/Managed/SagaScript.RuntimeBridge/"
                 "SagaScript.RuntimeBridge.dll");

    const auto paths = SagaEditor::ResolveEditorToolchain(root / "bin/Saga");

    EXPECT_EQ(paths.productRoot, root);
    EXPECT_EQ(paths.engineRoot, root);
    EXPECT_EQ(paths.starterArenaSeed, root / "samples/StarterArena");
    EXPECT_EQ(paths.sagaProjectKitExecutable, root / "bin/sagaproject");
    EXPECT_EQ(paths.sagaPackagerExecutable, root / "bin/sagapack");
    EXPECT_EQ(paths.runtimeBridge,
              root / "Engine/Managed/SagaScript.RuntimeBridge/"
                     "SagaScript.RuntimeBridge.dll");
    fs::remove_all(root);
}

TEST(SagaProjectKitClientTests, MissingTemplateFailsBeforeProcessWithDiagnostic)
{
    const auto root = TempRoot("missing-template");
    SagaEditor::ProjectTemplateRequest request;
    request.destination = root / "Created";
    request.displayName = "Created";
    request.seed = root / "missing-seed";
    request.sagaPackagerExecutable = root / "missing-packager";
    request.engineRoot = root;
    request.runtimeBridge = root / "missing-bridge";

    const auto result =
        SagaEditor::SagaProjectKitClient("unused").CreateStarterArena(request);

    ASSERT_FALSE(result.passed);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "Project.Template.SeedMissing");
    EXPECT_EQ(result.diagnostics.front().path, request.seed);
    EXPECT_EQ(result.process.exitCode, -1);
    fs::remove_all(root);
}

TEST(SagaProjectKitClientTests, MissingProjectToolHasPreciseDiagnostic)
{
    const auto root = TempRoot("missing-tool");
    SagaEditor::ProjectTemplateRequest request;
    request.destination = root / "Created";
    request.displayName = "Created";
    request.seed = root / "seed";
    request.sagaPackagerExecutable = root / "sagapack";
    request.engineRoot = root;
    request.runtimeBridge = root / "bridge.dll";

    const auto missingTool = root / "bin/sagaproject";
    const auto result =
        SagaEditor::SagaProjectKitClient(missingTool).CreateStarterArena(request);

    ASSERT_FALSE(result.passed);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code,
              "Project.Toolchain.ProjectToolMissing");
    EXPECT_EQ(result.diagnostics.front().path, missingTool);
    fs::remove_all(root);
}
