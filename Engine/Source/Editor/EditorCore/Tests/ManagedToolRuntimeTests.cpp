// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Processes/ManagedToolRuntime.h"
#include "SagaEditor/Documents/Document.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace
{
namespace fs = std::filesystem;

fs::path Root(std::string_view name)
{
    const auto root = fs::temp_directory_path() /
        ("saga-managed-runtime-" + std::string(name) + "-" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    fs::create_directories(root);
    return root;
}

void Write(const fs::path& path, std::string_view text)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

fs::path Stage(std::string_view name)
{
    const auto root = Root(name);
    Write(root / "managed-tool-runtime.json", R"({
      "schemaVersion": 1,
      "framework": "Microsoft.NETCore.App",
      "targetFramework": "net10.0",
      "minimumRuntimeVersion": "10.0.0",
      "runtimeVersion": "10.0.10",
      "rid": "linux-x64",
      "tools": [
        {"id":"ProjectKit","directory":"SagaProjectKit","assembly":"sagaproject"},
        {"id":"Script","directory":"SagaScript","assembly":"sagascript"},
        {"id":"Packager","directory":"SagaPackager","assembly":"sagapack"}
      ]
    })");
    const auto host = root / "runtime/dotnet/dotnet";
    Write(host,
        "#!/bin/sh\n"
        "printf 'dll=%s\\narg=%s\\nroot=%s\\nhost=%s\\nlookup=%s\\ninvariant=%s\\n' "
        "\"$1\" \"$2\" \"$DOTNET_ROOT\" \"$DOTNET_HOST_PATH\" "
        "\"$DOTNET_MULTILEVEL_LOOKUP\" "
        "\"$DOTNET_SYSTEM_GLOBALIZATION_INVARIANT\"\n");
    fs::permissions(host, fs::perms::owner_exec | fs::perms::owner_read |
        fs::perms::owner_write, fs::perm_options::replace);
    fs::create_directories(root /
        "runtime/dotnet/shared/Microsoft.NETCore.App/10.0.10");
    fs::create_directories(root / "runtime/dotnet/host/fxr/10.0.10");
    for (const auto* tool : {"SagaProjectKit/sagaproject",
                             "SagaScript/sagascript",
                             "SagaPackager/sagapack"})
    {
        const fs::path base = root / "tools" / tool;
        Write(base.string() + ".dll", "assembly\n");
        Write(base.string() + ".deps.json", "{}\n");
        Write(base.string() + ".runtimeconfig.json",
            R"({"runtimeOptions":{"tfm":"net10.0","framework":{"name":"Microsoft.NETCore.App","version":"10.0.0"}}})");
    }
    return root;
}

std::map<fs::path, std::string> Inventory(const fs::path& root)
{
    std::map<fs::path, std::string> result;
    for (const auto& entry : fs::recursive_directory_iterator(root))
        if (entry.is_regular_file())
        {
            std::error_code error;
            const auto bytes = SagaEditor::ReadDocumentBytes(entry.path(), error);
            EXPECT_FALSE(error);
            result.emplace(fs::relative(entry.path(), root),
                SagaEditor::HashDocumentBytes(bytes));
        }
    return result;
}
}

#ifndef _WIN32
TEST(ManagedToolRuntimeTests, UsesPrivateHostDllAndOverridesConflictingEnvironment)
{
    const auto stage = Stage("invocation");
    setenv("DOTNET_ROOT", "/hostile/global/runtime", 1);
    setenv("DOTNET_HOST_PATH", "/hostile/global/dotnet", 1);
    SagaEditor::ManagedToolRuntime runtime({{}, stage, {}});
    const auto result = runtime.Run(
        SagaEditor::ManagedToolId::Script, {"analyze"});
    ASSERT_TRUE(result.Succeeded()) << result.process.standardError;
    EXPECT_EQ(result.descriptor.resolutionMode,
              SagaEditor::ManagedToolResolutionMode::TestStage);
    EXPECT_NE(result.process.standardOutput.find(
        "dll=" + (stage / "tools/SagaScript/sagascript.dll").string()),
        std::string::npos);
    EXPECT_NE(result.process.standardOutput.find("arg=analyze"), std::string::npos);
    EXPECT_NE(result.process.standardOutput.find(
        "root=" + (stage / "runtime/dotnet").string()), std::string::npos);
    EXPECT_NE(result.process.standardOutput.find(
        "host=" + (stage / "runtime/dotnet/dotnet").string()), std::string::npos);
    EXPECT_NE(result.process.standardOutput.find("lookup=0"), std::string::npos);
    EXPECT_NE(result.process.standardOutput.find("invariant=1"), std::string::npos);
    fs::remove_all(stage);
}

TEST(ManagedToolRuntimeTests, ResolvesInstalledLayoutFromExecutableNotCwdOrPath)
{
    const auto stage = Stage("installed");
    const auto unrelated = Root("unrelated-cwd");
    const auto hostileBin = Root("hostile-path");
    Write(stage / "bin/Saga", "product anchor\n");
    const auto fakeDotnet = hostileBin / "dotnet";
    Write(fakeDotnet, "#!/bin/sh\nexit 91\n");
    fs::permissions(fakeDotnet, fs::perms::owner_exec | fs::perms::owner_read |
        fs::perms::owner_write, fs::perm_options::replace);

    const auto originalCwd = fs::current_path();
    const auto* originalPathValue = std::getenv("PATH");
    const std::string originalPath = originalPathValue ? originalPathValue : "";
    fs::current_path(unrelated);
    setenv("PATH", hostileBin.c_str(), 1);

    SagaEditor::ManagedToolRuntime runtime(
        {stage / "bin/Saga", {}, {}});
    const auto result = runtime.Run(
        SagaEditor::ManagedToolId::ProjectKit, {"validate"});

    fs::current_path(originalCwd);
    setenv("PATH", originalPath.c_str(), 1);
    ASSERT_TRUE(result.Succeeded()) << result.process.standardError;
    EXPECT_EQ(result.descriptor.resolutionMode,
              SagaEditor::ManagedToolResolutionMode::Installed);
    EXPECT_EQ(result.descriptor.trustedRoot, fs::canonical(stage));
    EXPECT_NE(result.process.standardOutput.find(
        "dll=" + (stage / "tools/SagaProjectKit/sagaproject.dll").string()),
        std::string::npos);
    EXPECT_NE(result.process.standardOutput.find("arg=validate"),
              std::string::npos);

    fs::remove_all(stage);
    fs::remove_all(unrelated);
    fs::remove_all(hostileBin);
}

TEST(ManagedToolRuntimeTests, UsesExplicitDeveloperHostWithRepoPublishedDll)
{
    const auto runtimeStage = Stage("developer-host-runtime");
    const auto repository = Root("developer-repository");
    Write(repository / "build/bin/Saga", "product anchor\n");
    fs::create_directories(repository / "Config");
    fs::copy_file(runtimeStage / "managed-tool-runtime.json",
                  repository / "Config/managed-tool-runtime.json");
    fs::create_directories(repository /
        "Tools/SagaScript/bin/Release/net10.0");
    fs::copy(runtimeStage / "tools/SagaScript",
             repository / "Tools/SagaScript/bin/Release/net10.0",
             fs::copy_options::recursive |
                 fs::copy_options::overwrite_existing);

    SagaEditor::ManagedToolRuntime runtime({
        repository / "build/bin/Saga",
        {},
        runtimeStage / "runtime/dotnet/dotnet",
    });
    const auto result = runtime.Run(
        SagaEditor::ManagedToolId::Script, {"analyze"});

    ASSERT_TRUE(result.Succeeded()) << result.process.standardError;
    EXPECT_EQ(result.descriptor.resolutionMode,
              SagaEditor::ManagedToolResolutionMode::DeveloperHost);
    EXPECT_EQ(result.descriptor.trustedRoot, fs::canonical(repository));
    EXPECT_EQ(result.descriptor.host,
              fs::canonical(runtimeStage / "runtime/dotnet/dotnet"));
    EXPECT_NE(result.process.standardOutput.find(
        "dll=" + (repository /
            "Tools/SagaScript/bin/Release/net10.0/sagascript.dll").string()),
        std::string::npos);

    fs::remove_all(runtimeStage);
    fs::remove_all(repository);
}

TEST(ManagedToolRuntimeTests, ReportsStableMissingAndMetadataDiagnostics)
{
    const auto missing = Root("missing");
    SagaEditor::ManagedToolRuntime missingRuntime({{}, missing, {}});
    auto result = missingRuntime.Run(SagaEditor::ManagedToolId::Packager, {});
    ASSERT_EQ(result.preflightDiagnostics.size(), 1U);
    EXPECT_EQ(result.preflightDiagnostics.front().code,
              "Product.Toolchain.RuntimeMissing");

    const auto stage = Stage("metadata");
    fs::remove(stage / "tools/SagaPackager/sagapack.deps.json");
    SagaEditor::ManagedToolRuntime metadataRuntime({{}, stage, {}});
    result = metadataRuntime.Run(SagaEditor::ManagedToolId::Packager, {});
    ASSERT_EQ(result.preflightDiagnostics.size(), 1U);
    EXPECT_EQ(result.preflightDiagnostics.front().code,
              "Product.Toolchain.ToolMetadataMissing");

    const auto missingToolStage = Stage("missing-tool");
    fs::remove(missingToolStage / "tools/SagaScript/sagascript.dll");
    SagaEditor::ManagedToolRuntime missingToolRuntime(
        {{}, missingToolStage, {}});
    result = missingToolRuntime.Run(SagaEditor::ManagedToolId::Script, {});
    ASSERT_EQ(result.preflightDiagnostics.size(), 1U);
    EXPECT_EQ(result.preflightDiagnostics.front().code,
              "Product.Toolchain.ToolMissing");
    fs::remove_all(missing);
    fs::remove_all(stage);
    fs::remove_all(missingToolStage);
}

TEST(ManagedToolRuntimeTests, RevalidatesCompleteCapturedInventoryBeforeEveryLaunch)
{
    const auto stage = Stage("captured-inventory");
    SagaEditor::ManagedToolRuntime runtime({{}, stage, {}, Inventory(stage)});
    ASSERT_TRUE(runtime.Run(SagaEditor::ManagedToolId::ProjectKit, {}).Succeeded());
    Write(stage / "tools/SagaScript/sagascript.dll", "changed\n");
    const auto changed = runtime.Run(SagaEditor::ManagedToolId::Script, {});
    ASSERT_EQ(changed.preflightDiagnostics.size(), 1U);
    EXPECT_EQ(changed.preflightDiagnostics.front().code,
              "Product.Toolchain.InventoryMismatch");
    fs::remove_all(stage);
}

TEST(ManagedToolRuntimeTests, RejectsEscapingToolPayload)
{
    const auto stage = Stage("escape");
    const auto external = Root("external");
    Write(external / "sagaproject.dll", "outside\n");
    fs::remove(stage / "tools/SagaProjectKit/sagaproject.dll");
    fs::create_symlink(external / "sagaproject.dll",
                       stage / "tools/SagaProjectKit/sagaproject.dll");
    SagaEditor::ManagedToolRuntime runtime({{}, stage, {}});
    const auto result = runtime.Run(SagaEditor::ManagedToolId::ProjectKit, {});
    ASSERT_EQ(result.preflightDiagnostics.size(), 1U);
    EXPECT_EQ(result.preflightDiagnostics.front().code,
              "Product.Toolchain.RuntimeCorrupt");
    fs::remove_all(stage);
    fs::remove_all(external);
}

TEST(ManagedToolRuntimeTests, RejectsIncompatibleAndMalformedMetadata)
{
    auto stage = Stage("incompatible");
    Write(stage / "managed-tool-runtime.json",
          R"({"schemaVersion":1,"framework":"Microsoft.NETCore.App","targetFramework":"net10.0","minimumRuntimeVersion":"10.0.0","runtimeVersion":"10.0.11","rid":"linux-x64","tools":[]})");
    SagaEditor::ManagedToolRuntime runtime({{}, stage, {}});
    auto result = runtime.Run(SagaEditor::ManagedToolId::Script, {});
    ASSERT_EQ(result.preflightDiagnostics.size(), 1U);
    EXPECT_EQ(result.preflightDiagnostics.front().code,
              "Product.Toolchain.RuntimeIncompatible");
    fs::remove_all(stage);

    stage = Stage("malformed");
    Write(stage / "tools/SagaScript/sagascript.runtimeconfig.json", "{");
    runtime = SagaEditor::ManagedToolRuntime({{}, stage, {}});
    result = runtime.Run(SagaEditor::ManagedToolId::Script, {});
    ASSERT_EQ(result.preflightDiagnostics.size(), 1U);
    EXPECT_EQ(result.preflightDiagnostics.front().code,
              "Product.Toolchain.ToolMetadataMissing");
    fs::remove_all(stage);
}
#endif
