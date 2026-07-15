/// @file StarterArenaRuntimeSmokeTests.cpp
/// @brief Focused StarterArena runtime smoke evidence tests.

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace
{

void WriteFile(const std::filesystem::path& path, const std::string& contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << contents;
}

[[nodiscard]] std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

[[nodiscard]] nlohmann::json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path);
    nlohmann::json value;
    input >> value;
    return value;
}

void SetEnvironment(const char* name, const std::filesystem::path& value)
{
#if defined(_WIN32)
    _putenv_s(name, value.string().c_str());
#else
    setenv(name, value.string().c_str(), 1);
#endif
}

void SetEnvironment(const char* name, const char* value)
{
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

void SetEnvironment(const char* name, const std::string& value)
{
    SetEnvironment(name, value.c_str());
}

[[nodiscard]] std::string ShellQuote(std::string_view value)
{
#if defined(_WIN32)
    std::string quoted = "\"";
    for (const char character : value)
    {
        if (character == '"')
        {
            quoted += "\\\"";
        }
        else
        {
            quoted.push_back(character);
        }
    }
    quoted.push_back('"');
    return quoted;
#else
    std::string quoted = "'";
    for (const char character : value)
    {
        if (character == '\'')
        {
            quoted += "'\\''";
        }
        else
        {
            quoted.push_back(character);
        }
    }
    quoted.push_back('\'');
    return quoted;
#endif
}

[[nodiscard]] std::string ShellQuote(const std::filesystem::path& path)
{
    const auto value = path.string();
    return ShellQuote(std::string_view(value));
}

[[nodiscard]] int RunCommand(const std::string& command)
{
    const int status = std::system(command.c_str());
#if defined(_WIN32)
    return status;
#else
    if (status == -1)
    {
        return status;
    }
    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }
    return status;
#endif
}

[[nodiscard]] std::filesystem::path SourceRoot()
{
    return std::filesystem::path(SAGA_SOURCE_ROOT);
}

[[nodiscard]] std::filesystem::path StarterArenaRoot()
{
    return SourceRoot() / "Samples" / "StarterArena";
}

[[nodiscard]] std::filesystem::path SagaScriptExecutable()
{
    return SourceRoot() / "Tools" / "SagaScript" / "sagascript";
}

[[nodiscard]] std::filesystem::path SagaRuntimeExecutable()
{
    return std::filesystem::path(SAGA_RUNTIME_EXECUTABLE);
}

[[nodiscard]] std::filesystem::path RuntimeBridgeAssembly()
{
    return std::filesystem::path(SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY);
}

[[nodiscard]] std::filesystem::path DotnetExecutable()
{
    return std::filesystem::path(SAGA_DOTNET_EXECUTABLE);
}

void ConfigureDotnetEnvironment()
{
    SetEnvironment("SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY", RuntimeBridgeAssembly());
    SetEnvironment("DOTNET_ROOT", std::filesystem::path(SAGA_DOTNET_ROOT));
    SetEnvironment("DOTNET_CLI_HOME", "/tmp/sagascript-dotnet-home-runtime-smoke");
    SetEnvironment("NUGET_PACKAGES", std::filesystem::path(SAGA_NUGET_PACKAGES));
    SetEnvironment("DOTNET_CLI_TELEMETRY_OPTOUT", "1");

    const auto dotnetDirectory = DotnetExecutable().parent_path().string();
    const char* currentPath = std::getenv("PATH");
#if defined(_WIN32)
    constexpr const char* kPathSeparator = ";";
#else
    constexpr const char* kPathSeparator = ":";
#endif
    SetEnvironment(
        "PATH",
        dotnetDirectory + kPathSeparator +
            (currentPath == nullptr ? "" : currentPath));
}

[[nodiscard]] std::filesystem::path TempProjectRoot(const char* name)
{
    const auto root = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "Scripts");
    std::filesystem::create_directories(root / "Scenes");
    std::filesystem::create_directories(root / "Input");
    std::filesystem::create_directories(root / "Diagnostics");
    std::filesystem::create_directories(root / "Build" / "Reports");
    return root;
}

void CopyStarterArenaSample(const std::filesystem::path& root)
{
    std::filesystem::copy_file(
        StarterArenaRoot() / "StarterArena.sagaproj",
        root / "StarterArena.sagaproj",
        std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(
        StarterArenaRoot() / "Scripts" / "GameRules.cs",
        root / "Scripts" / "GameRules.cs",
        std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(
        StarterArenaRoot() / "Scenes" / "arena.scene.json",
        root / "Scenes" / "arena.scene.json",
        std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(
        StarterArenaRoot() / "Input" / "playable.synthetic-input.json",
        root / "Input" / "playable.synthetic-input.json",
        std::filesystem::copy_options::overwrite_existing);
}

[[nodiscard]] bool CompileStarterArenaScripts(const std::filesystem::path& root)
{
    ConfigureDotnetEnvironment();

    const auto manifests = root / "Build" / "Manifests";
    const auto artifacts = root / "Build" / "Artifacts" / "Scripts";
    const auto diagnostics =
        root / "Build" / "Reports" / "sagascript_diagnostics.json";
    const auto stdoutPath = root / "Build" / "Reports" / "sagascript.stdout";
    const auto stderrPath = root / "Build" / "Reports" / "sagascript.stderr";
    std::filesystem::create_directories(stdoutPath.parent_path());

    const std::string command =
        ShellQuote(SagaScriptExecutable()) +
        " compile --source " + ShellQuote(root / "Scripts") +
        " --out " + ShellQuote(manifests) +
        " --artifacts-out " + ShellQuote(artifacts) +
        " --project-root " + ShellQuote(root) +
        " --diagnostics " + ShellQuote(diagnostics) +
        " --assembly-name StarterArenaScripts" +
        " --json > " + ShellQuote(stdoutPath) +
        " 2> " + ShellQuote(stderrPath);

    const int exitCode = RunCommand(command);
    if (exitCode != 0)
    {
        ADD_FAILURE()
            << "stdout:\n" << ReadFile(stdoutPath)
            << "\nstderr:\n" << ReadFile(stderrPath)
            << "\ndiagnostics:\n" << ReadFile(diagnostics);
        return false;
    }

    return true;
}

[[nodiscard]] int RunStarterArenaSmoke(
    const std::filesystem::path& root,
    const std::filesystem::path& reportPath,
    std::string_view extraArgs)
{
    ConfigureDotnetEnvironment();

    const auto stdoutPath = root / "Build" / "Reports" / "runtime.stdout";
    const auto stderrPath = root / "Build" / "Reports" / "runtime.stderr";
    const std::string command =
        ShellQuote(SagaRuntimeExecutable()) +
        " --headless --project " + ShellQuote(root / "StarterArena.sagaproj") +
        " --starter-arena-smoke --smoke-report-out " + ShellQuote(reportPath) +
        " --smoke-frames 30 --fixed-dt 0.016" +
        (extraArgs.empty() ? "" : " " + std::string(extraArgs)) +
        " > " + ShellQuote(stdoutPath) +
        " 2> " + ShellQuote(stderrPath);
    return RunCommand(command);
}

[[nodiscard]] std::string MetadataArgs(const std::filesystem::path& root)
{
    return "--script-manifest " +
        ShellQuote(root / "Build" / "Manifests" / "script_bindings.json") +
        " --script-artifacts " +
        ShellQuote(root / "Build" / "Manifests" / "script_artifacts.json");
}

[[nodiscard]] bool ContainsString(
    const nlohmann::json& array,
    std::string_view expected)
{
    return std::any_of(
        array.begin(),
        array.end(),
        [expected](const nlohmann::json& value)
        {
            return value.is_string() &&
                value.get<std::string>() == expected;
        });
}

[[nodiscard]] bool ContainsDiagnosticCode(
    const nlohmann::json& diagnostics,
    std::string_view expected)
{
    return std::any_of(
        diagnostics.begin(),
        diagnostics.end(),
        [expected](const nlohmann::json& diagnostic)
        {
            return diagnostic.is_object() &&
                diagnostic.value("code", "") == expected;
        });
}

[[nodiscard]] std::filesystem::path ResolveProjectPath(
    const std::filesystem::path& root,
    const std::string& path)
{
    const std::filesystem::path value(path);
    if (value.is_absolute())
    {
        return value.lexically_normal();
    }
    return (root / value).lexically_normal();
}

[[nodiscard]] bool IsPathWithin(
    const std::filesystem::path& path,
    const std::filesystem::path& root)
{
    const auto absolutePath = std::filesystem::weakly_canonical(path);
    const auto absoluteRoot = std::filesystem::weakly_canonical(root);
    const auto mismatch = std::mismatch(
        absoluteRoot.begin(),
        absoluteRoot.end(),
        absolutePath.begin(),
        absolutePath.end());
    return mismatch.first == absoluteRoot.end();
}

[[nodiscard]] std::filesystem::path StarterArenaAssemblyPath(
    const std::filesystem::path& root)
{
    const auto artifacts =
        ReadJson(root / "Build" / "Manifests" / "script_artifacts.json");
    const auto& artifactArray = artifacts.at("artifacts");
    for (const auto& artifact : artifactArray)
    {
        if (artifact.value("scriptId", "") == "script://starter-arena/game-rules")
        {
            return ResolveProjectPath(
                root,
                artifact.value("assemblyPath", ""));
        }
    }
    return {};
}

} // namespace

TEST(StarterArenaRuntimeSmokeTests, DefaultSmokeReportsProjectSceneAndNoScriptExecution)
{
    const auto root =
        TempProjectRoot("saga_starter_arena_runtime_smoke_default");
    CopyStarterArenaSample(root);

    const auto reportPath =
        root / "Build" / "Reports" / "starter_arena_smoke.json";
    const int exitCode = RunStarterArenaSmoke(root, reportPath, "");
    ASSERT_EQ(exitCode, 0) << ReadFile(root / "Build" / "Reports" / "runtime.stderr");

    const auto report = ReadJson(reportPath);
    EXPECT_EQ(report.value("status", ""), "Passed");
    EXPECT_EQ(report["project"].value("projectId", ""), "starter-arena");
    EXPECT_EQ(report["project"].value("sceneSource", ""), "ProjectSceneReference");
    EXPECT_TRUE(report["project"].value("projectPath", "").find("StarterArena.sagaproj") !=
        std::string::npos);
    EXPECT_EQ(report["scene"].value("sceneId", ""), "starter-arena-local-loop");
    EXPECT_EQ(report["scene"].value("source", ""), "Scenes/arena.scene.json");
    EXPECT_DOUBLE_EQ(report["scene"]["playerSpawn"].value("x", 1.0), 0.0);
    EXPECT_TRUE(report["scene"].contains("bounds"));
    EXPECT_TRUE(report["scene"].contains("expected"));
    EXPECT_EQ(report["settings"].value("frames", 0), 30);
    EXPECT_DOUBLE_EQ(report["settings"].value("fixedDtSeconds", 0.0), 0.016);
    EXPECT_EQ(report["loop"].value("frames", 0), 30);
    EXPECT_DOUBLE_EQ(report["loop"].value("fixedDtSeconds", 0.0), 0.016);
    EXPECT_EQ(report["loop"].value("status", ""), "Passed");
    EXPECT_DOUBLE_EQ(report["loop"]["inputVector"].value("x", 0.0), 4.0);
    EXPECT_DOUBLE_EQ(report["loop"]["inputVector"].value("y", 0.0), 2.0);
    EXPECT_DOUBLE_EQ(report["loop"]["finalPosition"].value("x", 0.0), 1.0);
    EXPECT_NEAR(report["loop"]["finalPosition"].value("y", 0.0), 0.96, 0.000001);
    EXPECT_TRUE(report["loop"].contains("bounds"));
    EXPECT_EQ(report["loop"].value("clampCount", 0), 15);

    ASSERT_TRUE(report.contains("scriptBinding"));
    EXPECT_EQ(report["scriptBinding"].value("status", ""), "NotProvided");
    EXPECT_EQ(report["scriptBinding"].value("execution", ""), "NotProvided");
    ASSERT_TRUE(report.contains("scriptInvocation"));
    EXPECT_EQ(report["scriptInvocation"].value("status", ""), "NotRequested");
    EXPECT_EQ(report["scriptInvocation"].value("execution", ""), "NotRequested");
    ASSERT_TRUE(report.contains("scriptLifecycle"));
    EXPECT_EQ(report["scriptLifecycle"].value("status", ""), "NotRequested");
    EXPECT_EQ(report["scriptLifecycle"].value("execution", ""), "NotRequested");
    EXPECT_FALSE(report["scriptLifecycle"].value("attempted", true));
    EXPECT_TRUE(report["scriptLifecycle"]["callbacksObserved"].empty());
    EXPECT_EQ(report["gameplay"].value("status", ""), "NotRequested");
    EXPECT_FALSE(report["gameplay"].value("enabled", true));
    EXPECT_TRUE(report["gameplay"]["mutations"].empty());
    EXPECT_TRUE(report["diagnostics"].empty());
    EXPECT_TRUE(ContainsString(
        report["nonClaims"],
        "No C# script lifecycle execution"));
}

TEST(StarterArenaRuntimeSmokeTests, MetadataOnlySmokeAcceptsScriptArtifactsWithoutExecution)
{
    const auto root =
        TempProjectRoot("saga_starter_arena_runtime_smoke_metadata_only");
    CopyStarterArenaSample(root);
    ASSERT_TRUE(CompileStarterArenaScripts(root));

    const auto reportPath =
        root / "Build" / "Reports" / "starter_arena_metadata_smoke.json";
    const int exitCode =
        RunStarterArenaSmoke(root, reportPath, MetadataArgs(root));
    ASSERT_EQ(exitCode, 0) << ReadFile(root / "Build" / "Reports" / "runtime.stderr");

    const auto report = ReadJson(reportPath);
    EXPECT_EQ(report.value("status", ""), "Passed");
    ASSERT_TRUE(report.contains("scriptBinding"));
    EXPECT_EQ(report["scriptBinding"].value("status", ""), "Passed");
    EXPECT_EQ(report["scriptBinding"].value("execution", ""), "MetadataOnly");
    EXPECT_EQ(
        report["scriptBinding"].value("scriptId", ""),
        "script://starter-arena/game-rules");
    EXPECT_EQ(
        report["scriptBinding"].value("typeName", ""),
        "StarterArena.Scripts.GameRules");
    EXPECT_TRUE(ContainsString(
        report["scriptBinding"]["callableMethods"],
        "AddPickupScore"));

    ASSERT_TRUE(report.contains("scriptInvocation"));
    EXPECT_EQ(report["scriptInvocation"].value("status", ""), "NotRequested");
    EXPECT_EQ(report["scriptInvocation"].value("execution", ""), "NotRequested");
    EXPECT_FALSE(report["scriptInvocation"].value("attempted", true));
    ASSERT_TRUE(report.contains("scriptLifecycle"));
    EXPECT_EQ(report["scriptLifecycle"].value("status", ""), "NotRequested");
    EXPECT_EQ(report["scriptLifecycle"].value("execution", ""), "NotRequested");
    EXPECT_FALSE(report["scriptLifecycle"].value("attempted", true));
    EXPECT_EQ(report["gameplay"].value("status", ""), "NotRequested");
    EXPECT_TRUE(report["diagnostics"].empty());
}

TEST(StarterArenaRuntimeSmokeTests, ControlledInvocationSmokeInvokesOnlyAddPickupScore)
{
    const auto root =
        TempProjectRoot("saga_starter_arena_runtime_smoke_invocation");
    CopyStarterArenaSample(root);
    ASSERT_TRUE(CompileStarterArenaScripts(root));

    const auto reportPath =
        root / "Build" / "Reports" / "starter_arena_invocation_smoke.json";
    const std::string args =
        MetadataArgs(root) + " --invoke-starter-arena-script";
    const int exitCode = RunStarterArenaSmoke(root, reportPath, args);
    ASSERT_EQ(exitCode, 0) << ReadFile(root / "Build" / "Reports" / "runtime.stderr");

    const auto report = ReadJson(reportPath);
    EXPECT_EQ(report.value("status", ""), "Passed");
    EXPECT_EQ(report["scriptBinding"].value("status", ""), "Passed");
    EXPECT_EQ(report["scriptBinding"].value("execution", ""), "MetadataOnly");

    ASSERT_TRUE(report.contains("scriptInvocation"));
    const auto& invocation = report["scriptInvocation"];
    EXPECT_EQ(invocation.value("status", ""), "Passed");
    EXPECT_EQ(invocation.value("execution", ""), "Invoked");
    EXPECT_EQ(invocation.value("scriptId", ""), "script://starter-arena/game-rules");
    EXPECT_EQ(invocation.value("typeName", ""), "StarterArena.Scripts.GameRules");
    EXPECT_EQ(invocation.value("method", ""), "AddPickupScore");
    EXPECT_FALSE(invocation.contains("className"));
    EXPECT_FALSE(invocation.contains("requestedMethod"));
    ASSERT_TRUE(invocation["arguments"].is_array());
    ASSERT_EQ(invocation["arguments"].size(), 2U);
    EXPECT_EQ(invocation["arguments"][0].get<int>(), 10);
    EXPECT_EQ(invocation["arguments"][1].get<int>(), 5);
    EXPECT_EQ(invocation.value("result", 0), 15);
    EXPECT_TRUE(invocation.value("attempted", false));

    ASSERT_TRUE(report.contains("scriptLifecycle"));
    EXPECT_EQ(report["scriptLifecycle"].value("status", ""), "NotRequested");
    EXPECT_EQ(report["scriptLifecycle"].value("execution", ""), "NotRequested");
    EXPECT_TRUE(report["diagnostics"].empty());
}

TEST(StarterArenaRuntimeSmokeTests, LifecycleSmokeRunsGameRulesLifecycle)
{
    const auto root =
        TempProjectRoot("saga_starter_arena_runtime_smoke_lifecycle");
    CopyStarterArenaSample(root);
    ASSERT_TRUE(CompileStarterArenaScripts(root));

    const auto reportPath =
        root / "Build" / "Reports" / "starter_arena_lifecycle_smoke.json";
    const std::string args =
        MetadataArgs(root) + " --run-starter-arena-script-lifecycle";
    const int exitCode = RunStarterArenaSmoke(root, reportPath, args);
    ASSERT_EQ(exitCode, 0) << ReadFile(root / "Build" / "Reports" / "runtime.stderr");

    const auto report = ReadJson(reportPath);
    EXPECT_EQ(report.value("status", ""), "Passed");
    EXPECT_EQ(report["scriptBinding"].value("status", ""), "Passed");
    EXPECT_EQ(report["scriptBinding"].value("execution", ""), "MetadataOnly");
    EXPECT_EQ(report["scriptInvocation"].value("status", ""), "NotRequested");
    EXPECT_EQ(report["scriptInvocation"].value("execution", ""), "NotRequested");
    ASSERT_TRUE(report.contains("scriptLifecycle"));
    const auto& lifecycle = report["scriptLifecycle"];
    EXPECT_EQ(lifecycle.value("status", ""), "Passed");
    EXPECT_EQ(lifecycle.value("scriptId", ""), "script://starter-arena/game-rules");
    EXPECT_EQ(lifecycle.value("typeName", ""), "StarterArena.Scripts.GameRules");
    EXPECT_EQ(lifecycle.value("execution", ""), "Invoked");
    EXPECT_TRUE(lifecycle.value("attempted", false));
    EXPECT_TRUE(ContainsString(lifecycle["callbacksObserved"], "OnCreate"));
    EXPECT_TRUE(ContainsString(lifecycle["callbacksObserved"], "OnStart"));
    EXPECT_TRUE(ContainsString(lifecycle["callbacksObserved"], "OnUpdate"));
    EXPECT_TRUE(ContainsString(lifecycle["callbacksObserved"], "OnDestroy"));
    EXPECT_FALSE(ContainsString(
        report["nonClaims"],
        "No C# script lifecycle execution"));
    EXPECT_TRUE(report["diagnostics"].empty());
    EXPECT_FALSE(std::filesystem::exists(
        StarterArenaRoot() / "Build" / "Manifests" / "script_artifacts.json"));
}

TEST(StarterArenaRuntimeSmokeTests, GameplayLifecycleMutatesDeterministicState)
{
    const auto root = TempProjectRoot("saga_starter_arena_gameplay_lifecycle");
    CopyStarterArenaSample(root);
    ASSERT_TRUE(CompileStarterArenaScripts(root));
    const auto reportPath = std::filesystem::temp_directory_path() /
                            "saga_starter_arena_gameplay_lifecycle.json";
    std::filesystem::remove(reportPath);
    const std::string args = MetadataArgs(root) +
        " --run-starter-arena-script-lifecycle --run-starter-arena-gameplay";
    ASSERT_EQ(RunStarterArenaSmoke(root, reportPath, args), 0)
        << ReadFile(root / "Build" / "Reports" / "runtime.stderr");

    const auto report = ReadJson(reportPath);
    EXPECT_EQ(report["gameplay"].value("status", ""), "Passed");
    EXPECT_EQ(report["gameplay"]["finalState"].value("score", 0), 10);
    EXPECT_TRUE(report["gameplay"]["finalState"].value("pickupCollected", false));
    EXPECT_EQ(report["gameplay"]["finalState"].value("playerState", ""), "powered");
    EXPECT_EQ(report["gameplay"]["mutations"].size(), 3u);
    EXPECT_EQ(report["gameplay"]["mutations"][0].value("lifecycleEvent", ""), "OnUpdate");
    const auto& trigger = report["gameplay"]["trigger"]["playerPosition"];
    const auto& pickup = report["gameplay"]["pickup"];
    const double triggerDx = trigger.value("x", 0.0) - pickup["position"].value("x", 0.0);
    const double triggerDy = trigger.value("y", 0.0) - pickup["position"].value("y", 0.0);
    EXPECT_LE(triggerDx * triggerDx + triggerDy * triggerDy,
              pickup.value("radius", 0.0) * pickup.value("radius", 0.0));
    EXPECT_FALSE(std::abs(trigger.value("x", 0.0) -
                          report["loop"]["finalPosition"].value("x", 0.0)) <= 0.000001 &&
                 std::abs(trigger.value("y", 0.0) -
                          report["loop"]["finalPosition"].value("y", 0.0)) <= 0.000001);
    EXPECT_EQ(report["scriptLifecycle"].value("execution", ""), "Invoked");
    EXPECT_TRUE(report["diagnostics"].empty());
}

TEST(StarterArenaRuntimeSmokeTests, GameplayFlagRequiresLifecycle)
{
    const auto root = TempProjectRoot("saga_starter_arena_gameplay_flag_rejected");
    CopyStarterArenaSample(root);
    const auto reportPath = std::filesystem::temp_directory_path() /
                            "saga_starter_arena_gameplay_flag_rejected.json";
    std::filesystem::remove(reportPath);
    EXPECT_EQ(RunStarterArenaSmoke(root, reportPath, "--run-starter-arena-gameplay"), 2);
    EXPECT_FALSE(std::filesystem::exists(reportPath));
}

TEST(StarterArenaRuntimeSmokeTests, CombinedInvocationAndLifecycleSmokeReportsBothIndependently)
{
    const auto root =
        TempProjectRoot("saga_starter_arena_runtime_smoke_combined");
    CopyStarterArenaSample(root);
    ASSERT_TRUE(CompileStarterArenaScripts(root));

    const auto reportPath =
        root / "Build" / "Reports" / "starter_arena_combined_smoke.json";
    const std::string args =
        MetadataArgs(root) +
        " --invoke-starter-arena-script --run-starter-arena-script-lifecycle";
    const int exitCode = RunStarterArenaSmoke(root, reportPath, args);
    ASSERT_EQ(exitCode, 0) << ReadFile(root / "Build" / "Reports" / "runtime.stderr");

    const auto report = ReadJson(reportPath);
    EXPECT_EQ(report.value("status", ""), "Passed");
    EXPECT_EQ(report["scriptBinding"].value("status", ""), "Passed");
    EXPECT_EQ(report["scriptBinding"].value("execution", ""), "MetadataOnly");
    EXPECT_EQ(report["scriptInvocation"].value("status", ""), "Passed");
    EXPECT_EQ(report["scriptInvocation"].value("execution", ""), "Invoked");
    EXPECT_EQ(report["scriptInvocation"].value("method", ""), "AddPickupScore");
    EXPECT_EQ(report["scriptInvocation"].value("result", 0), 15);
    EXPECT_EQ(report["scriptLifecycle"].value("status", ""), "Passed");
    EXPECT_EQ(report["scriptLifecycle"].value("execution", ""), "Invoked");
    EXPECT_TRUE(ContainsString(
        report["scriptLifecycle"]["callbacksObserved"],
        "OnCreate"));
    EXPECT_TRUE(report["diagnostics"].empty());
}

TEST(StarterArenaRuntimeSmokeTests, LifecycleSmokeFailsWithoutMetadataBeforeExecution)
{
    const auto root =
        TempProjectRoot("saga_starter_arena_runtime_smoke_missing_metadata");
    CopyStarterArenaSample(root);

    const auto reportPath =
        root / "Build" / "Reports" / "starter_arena_missing_metadata.json";
    const int exitCode = RunStarterArenaSmoke(
        root,
        reportPath,
        "--run-starter-arena-script-lifecycle");
    ASSERT_NE(exitCode, 0);

    const auto report = ReadJson(reportPath);
    EXPECT_EQ(report.value("status", ""), "Failed");
    ASSERT_TRUE(report.contains("scriptLifecycle"));
    EXPECT_EQ(report["scriptLifecycle"].value("status", ""), "Failed");
    EXPECT_EQ(report["scriptLifecycle"].value("execution", ""), "NotExecuted");
    EXPECT_FALSE(report["scriptLifecycle"].value("attempted", true));
    EXPECT_TRUE(ContainsDiagnosticCode(
        report["diagnostics"],
        "StarterArena.ScriptLifecycle.MetadataRequired"));
}

TEST(StarterArenaRuntimeSmokeTests, InvalidArtifactManifestFailsClearly)
{
    const auto root =
        TempProjectRoot("saga_starter_arena_runtime_smoke_invalid_artifacts");
    CopyStarterArenaSample(root);
    ASSERT_TRUE(CompileStarterArenaScripts(root));
    WriteFile(
        root / "Build" / "Manifests" / "script_artifacts.json",
        "{\n  \"targetFramework\": \"net10.0\",\n  \"artifacts\": {}\n}\n");

    const auto reportPath =
        root / "Build" / "Reports" / "starter_arena_invalid_artifacts.json";
    const int exitCode =
        RunStarterArenaSmoke(root, reportPath, MetadataArgs(root));
    ASSERT_NE(exitCode, 0);

    const auto report = ReadJson(reportPath);
    EXPECT_EQ(report.value("status", ""), "Failed");
    EXPECT_EQ(report["scriptBinding"].value("status", ""), "Failed");
    EXPECT_EQ(report["scriptBinding"].value("execution", ""), "Invalid");
    EXPECT_EQ(report["scriptInvocation"].value("status", ""), "NotRequested");
    EXPECT_EQ(report["scriptLifecycle"].value("status", ""), "NotRequested");
    EXPECT_TRUE(ContainsDiagnosticCode(
        report["diagnostics"],
        "StarterArena.ScriptBinding.ArtifactsInvalid"));
}

TEST(StarterArenaRuntimeSmokeTests, GeneratedArtifactsStayOutsideStarterArenaSample)
{
    const auto root =
        TempProjectRoot("saga_starter_arena_runtime_smoke_artifact_hygiene");
    CopyStarterArenaSample(root);
    ASSERT_TRUE(CompileStarterArenaScripts(root));

    const auto reportPath =
        root / "Build" / "Reports" / "starter_arena_artifact_hygiene.json";
    const int exitCode =
        RunStarterArenaSmoke(root, reportPath, MetadataArgs(root));
    ASSERT_EQ(exitCode, 0) << ReadFile(root / "Build" / "Reports" / "runtime.stderr");

    const auto manifestPath =
        root / "Build" / "Manifests" / "script_bindings.json";
    const auto artifactsPath =
        root / "Build" / "Manifests" / "script_artifacts.json";
    const auto assemblyPath = StarterArenaAssemblyPath(root);
    ASSERT_FALSE(assemblyPath.empty());

    EXPECT_TRUE(std::filesystem::exists(manifestPath));
    EXPECT_TRUE(std::filesystem::exists(artifactsPath));
    EXPECT_TRUE(std::filesystem::exists(assemblyPath));
    EXPECT_TRUE(std::filesystem::exists(
        assemblyPath.parent_path() / "SagaScript.RuntimeBridge.dll"));
    EXPECT_TRUE(std::filesystem::exists(reportPath));

    EXPECT_TRUE(IsPathWithin(manifestPath, root));
    EXPECT_TRUE(IsPathWithin(artifactsPath, root));
    EXPECT_TRUE(IsPathWithin(assemblyPath, root));
    EXPECT_TRUE(IsPathWithin(reportPath, root));

    const auto sampleRoot = StarterArenaRoot();
    EXPECT_FALSE(std::filesystem::exists(
        sampleRoot / "Build" / "Manifests" / "script_bindings.json"));
    EXPECT_FALSE(std::filesystem::exists(
        sampleRoot / "Build" / "Manifests" / "script_artifacts.json"));
    EXPECT_FALSE(std::filesystem::exists(
        sampleRoot / "Build" / "Artifacts" / "Scripts" /
        "StarterArenaScripts.scripts.dll"));
    EXPECT_FALSE(std::filesystem::exists(
        sampleRoot / "Build" / "Reports" / "starter_arena_artifact_hygiene.json"));
}

TEST(StarterArenaRuntimeSmokeTests, VisibleModeRejectsHeadlessBeforeExecution)
{
    const auto root =
        TempProjectRoot("saga_starter_arena_visible_headless_rejected");
    CopyStarterArenaSample(root);
    const auto reportPath =
        std::filesystem::temp_directory_path() /
        "saga_starter_arena_visible_headless_rejected.json";
    std::filesystem::remove(reportPath);

    const std::string command =
        ShellQuote(SagaRuntimeExecutable()) + " --headless --project " +
        ShellQuote(root / "StarterArena.sagaproj") +
        " --starter-arena-playable --playable-frames 2" +
        " --playable-report-out " + ShellQuote(reportPath);
    ASSERT_NE(RunCommand(command), 0);
    ASSERT_TRUE(std::filesystem::exists(reportPath));

    const auto report = ReadJson(reportPath);
    EXPECT_EQ(report.value("status", ""), "Failed");
    EXPECT_EQ(report["render"].value("status", ""), "NotInitialized");
    EXPECT_EQ(report["render"].value("presentedFrames", 1u), 0u);
    EXPECT_TRUE(ContainsDiagnosticCode(
        report["diagnostics"],
        "StarterArena.Playable.VisibleRequired"));
}

TEST(StarterArenaRuntimeSmokeTests,
     PlayableInputFlagsWithoutPlayableModeFailBeforeRuntimeStartup)
{
    const auto root =
        TempProjectRoot("saga_starter_arena_playable_flags_without_mode");
    const auto scriptPath = root / "Input" / "unused.json";
    WriteFile(scriptPath, "{}");

    const auto runRejectedCommand = [&](const std::string& arguments,
                                        const char* outputName,
                                        const char* reportName) {
        const auto outputPath = root / "Build" / "Reports" / outputName;
        const auto reportPath = root / "Build" / "Reports" / reportName;
        std::filesystem::remove(reportPath);
        const std::string command =
            ShellQuote(SagaRuntimeExecutable()) + " " + arguments +
            " --playable-report-out " + ShellQuote(reportPath) +
            " > " + ShellQuote(outputPath) + " 2>&1";

        EXPECT_EQ(RunCommand(command), 2);
        const std::string output = ReadFile(outputPath);
        EXPECT_NE(output.find("--playable-input-source and "
                              "--playable-input-script require "
                              "--starter-arena-playable."),
                  std::string::npos);
        EXPECT_EQ(output.find("Runtime service registry startup"),
                  std::string::npos);
        EXPECT_FALSE(std::filesystem::exists(reportPath));
    };

    runRejectedCommand("--playable-input-source synthetic",
                       "source.stdout",
                       "source-report.json");
    runRejectedCommand("--playable-input-script " + ShellQuote(scriptPath),
                       "script.stdout",
                       "script-report.json");
}

TEST(StarterArenaRuntimeSmokeTests, BoundedVisibleModeWritesStableReport)
{
#if !defined(_WIN32)
    if (std::getenv("DISPLAY") == nullptr &&
        std::getenv("WAYLAND_DISPLAY") == nullptr)
    {
        GTEST_SKIP() << "No display is available for bounded visible evidence.";
    }
#endif
    const auto root = TempProjectRoot("saga_starter_arena_visible_bounded");
    CopyStarterArenaSample(root);
    const auto reportPath =
        std::filesystem::temp_directory_path() /
        "saga_starter_arena_visible_bounded.json";
    std::filesystem::remove(reportPath);
    const auto stdoutPath = root / "Build" / "Reports" / "playable.stdout";
    const auto stderrPath = root / "Build" / "Reports" / "playable.stderr";

    const std::string command =
        ShellQuote(SagaRuntimeExecutable()) + " --project " +
        ShellQuote(root / "StarterArena.sagaproj") +
        " --starter-arena-playable --playable-frames 30" +
        " --playable-report-out " + ShellQuote(reportPath) +
        " --fixed-dt 0.016 > " + ShellQuote(stdoutPath) +
        " 2> " + ShellQuote(stderrPath);
    ASSERT_EQ(RunCommand(command), 0)
        << ReadFile(stdoutPath) << "\n" << ReadFile(stderrPath);

    const auto report = ReadJson(reportPath);
    EXPECT_EQ(report.value("status", ""), "Passed");
    EXPECT_EQ(report["project"].value("projectId", ""), "starter-arena");
    EXPECT_EQ(report["scene"].value("sceneId", ""),
              "starter-arena-local-loop");
    EXPECT_EQ(report["simulation"].value("ticks", 0u), 30u);
    EXPECT_EQ(report["input"].value("status", ""), "Passed");
    EXPECT_EQ(report["input"].value("source", ""), "scene");
    EXPECT_EQ(report["input"].value("framesWithInput", 0u), 30u);
    EXPECT_FALSE(report["input"].value("realDeviceObserved", true));
    EXPECT_EQ(report["gameplay"].value("status", ""), "NotRequested");
    EXPECT_FALSE(report["gameplay"].value("enabled", true));
    EXPECT_TRUE(ContainsString(report["nonClaims"], "No C# world mutation proof"));
    const auto& mapping = report["input"]["mapping"];
    EXPECT_EQ(mapping.value("MoveLeft", ""), "A");
    EXPECT_EQ(mapping.value("MoveRight", ""), "D");
    EXPECT_EQ(mapping.value("MoveUp", ""), "W");
    EXPECT_EQ(mapping.value("MoveDown", ""), "S");
    EXPECT_EQ(mapping.value("Quit", ""), "Escape");
    EXPECT_EQ(mapping["keys"]["MoveLeft"], mapping["MoveLeft"]);
    EXPECT_EQ(mapping["keys"]["MoveRight"], mapping["MoveRight"]);
    EXPECT_EQ(mapping["keys"]["MoveUp"], mapping["MoveUp"]);
    EXPECT_EQ(mapping["keys"]["MoveDown"], mapping["MoveDown"]);
    EXPECT_EQ(mapping["keys"]["Quit"], mapping["Quit"]);
    EXPECT_EQ(mapping.value("axisConvention", ""),
              "positiveXRight_positiveYUpForward");
    EXPECT_EQ(mapping.value("countSemantics", ""), "activeTicks");
    EXPECT_EQ(mapping.value("diagonalPolicy", ""),
              "normalizedForSyntheticAndKeyboard");
    EXPECT_DOUBLE_EQ(mapping.value("actionMovementSpeed", 0.0), 2.0);
    EXPECT_EQ(mapping.value("sceneInputSemantics", ""),
              "authoredVelocityPreserved");
    EXPECT_DOUBLE_EQ(
        report["simulation"]["finalPosition"].value("x", 0.0), 1.0);
    EXPECT_NEAR(
        report["simulation"]["finalPosition"].value("y", 0.0),
        0.96,
        0.000001);
    EXPECT_EQ(report["render"].value("status", ""), "Passed");
    EXPECT_TRUE(report["render"].value("initialized", false));
    EXPECT_EQ(report["render"].value("presentedFrames", 0u), 30u);
    EXPECT_GT(report["render"].value("totalDrawItems", 0u), 0u);
    EXPECT_TRUE(report["render"].value("playerDrawSubmitted", false));
    EXPECT_EQ(report["render"].value("shutdown", ""), "Completed");
    EXPECT_TRUE(report["diagnostics"].empty());
    EXPECT_FALSE(IsPathWithin(reportPath, StarterArenaRoot()));
}

TEST(StarterArenaRuntimeSmokeTests, BoundedSyntheticVisibleModeWritesInputEvidence)
{
#if !defined(_WIN32)
    if (std::getenv("DISPLAY") == nullptr &&
        std::getenv("WAYLAND_DISPLAY") == nullptr)
    {
        GTEST_SKIP() << "No display is available for bounded visible evidence.";
    }
#endif
    const auto root = TempProjectRoot("saga_starter_arena_visible_synthetic");
    CopyStarterArenaSample(root);
    const auto reportPath = std::filesystem::temp_directory_path() /
                            "saga_starter_arena_visible_synthetic.json";
    std::filesystem::remove(reportPath);
    const auto stdoutPath = root / "Build" / "Reports" / "synthetic.stdout";
    const auto stderrPath = root / "Build" / "Reports" / "synthetic.stderr";
    const std::string command =
        ShellQuote(SagaRuntimeExecutable()) + " --project " +
        ShellQuote(root / "StarterArena.sagaproj") +
        " --starter-arena-playable --playable-frames 30" +
        " --playable-input-source synthetic --playable-input-script " +
        ShellQuote(root / "Input" / "playable.synthetic-input.json") +
        " --playable-report-out " + ShellQuote(reportPath) +
        " --fixed-dt 0.016 > " + ShellQuote(stdoutPath) +
        " 2> " + ShellQuote(stderrPath);
    ASSERT_EQ(RunCommand(command), 0)
        << ReadFile(stdoutPath) << "\n" << ReadFile(stderrPath);

    const auto report = ReadJson(reportPath);
    EXPECT_EQ(report.value("status", ""), "Passed");
    EXPECT_EQ(report["input"].value("status", ""), "Passed");
    EXPECT_EQ(report["input"].value("source", ""), "synthetic");
    EXPECT_EQ(report["input"].value("framesWithInput", 0u), 30u);
    EXPECT_EQ(report["input"]["actionsObserved"].value("MoveRight", 0u), 15u);
    EXPECT_EQ(report["input"]["actionsObserved"].value("MoveUp", 0u), 15u);
    EXPECT_FALSE(report["input"].value("realDeviceObserved", true));
    EXPECT_NEAR(report["simulation"]["finalPosition"].value("x", 0.0),
                0.48,
                0.000001);
    EXPECT_NEAR(report["simulation"]["finalPosition"].value("y", 0.0),
                0.48,
                0.000001);
    EXPECT_GT(report["render"].value("totalDrawItems", 0u), 0u);
    EXPECT_TRUE(report["render"].value("playerDrawSubmitted", false));
}

TEST(StarterArenaRuntimeSmokeTests, VisibleSyntheticGameplayReflectsManagedMutation)
{
#if !defined(_WIN32)
    if (std::getenv("DISPLAY") == nullptr && std::getenv("WAYLAND_DISPLAY") == nullptr)
        GTEST_SKIP() << "No display is available for visible gameplay evidence.";
#endif
    const auto root = TempProjectRoot("saga_starter_arena_visible_gameplay");
    CopyStarterArenaSample(root);
    ASSERT_TRUE(CompileStarterArenaScripts(root));
    const auto reportPath = std::filesystem::temp_directory_path() /
                            "saga_starter_arena_visible_gameplay.json";
    std::filesystem::remove(reportPath);
    const auto stdoutPath = root / "Build" / "Reports" / "gameplay.stdout";
    const auto stderrPath = root / "Build" / "Reports" / "gameplay.stderr";
    const std::string command = ShellQuote(SagaRuntimeExecutable()) + " --project " +
        ShellQuote(root / "StarterArena.sagaproj") +
        " --starter-arena-playable --playable-frames 30" +
        " --playable-input-source synthetic --playable-input-script " +
        ShellQuote(root / "Input" / "playable.synthetic-input.json") +
        " --run-starter-arena-script-lifecycle --run-starter-arena-gameplay " +
        MetadataArgs(root) + " --playable-report-out " + ShellQuote(reportPath) +
        " --fixed-dt 0.016 > " + ShellQuote(stdoutPath) + " 2> " + ShellQuote(stderrPath);
    ASSERT_EQ(RunCommand(command), 0) << ReadFile(stdoutPath) << "\n" << ReadFile(stderrPath);

    const auto report = ReadJson(reportPath);
    EXPECT_EQ(report["gameplay"].value("status", ""), "Passed");
    EXPECT_EQ(report["gameplay"]["finalState"].value("score", 0), 10);
    EXPECT_TRUE(report["gameplay"]["finalState"].value("pickupCollected", false));
    EXPECT_EQ(report["gameplay"]["finalState"].value("playerState", ""), "powered");
    EXPECT_TRUE(report["render"].value("pickupDrawSubmittedBeforeCollection", false));
    EXPECT_FALSE(report["render"].value("pickupDrawSubmittedLastFrame", true));
    EXPECT_TRUE(report["render"].value("poweredPlayerDrawSubmitted", false));
    EXPECT_TRUE(report["render"].value("gameplayStateReflected", false));
    EXPECT_FALSE(ContainsString(report["nonClaims"], "No C# world mutation proof"));
    EXPECT_TRUE(ContainsString(report["nonClaims"],
                               "No broad gameplay scripting API claim"));
    const auto& trigger = report["gameplay"]["trigger"]["playerPosition"];
    const auto& pickup = report["gameplay"]["pickup"];
    const double triggerDx = trigger.value("x", 0.0) - pickup["position"].value("x", 0.0);
    const double triggerDy = trigger.value("y", 0.0) - pickup["position"].value("y", 0.0);
    EXPECT_LE(triggerDx * triggerDx + triggerDy * triggerDy,
              pickup.value("radius", 0.0) * pickup.value("radius", 0.0));
    EXPECT_FALSE(std::abs(trigger.value("x", 0.0) -
                          report["simulation"]["finalPosition"].value("x", 0.0)) <= 0.000001 &&
                 std::abs(trigger.value("y", 0.0) -
                          report["simulation"]["finalPosition"].value("y", 0.0)) <= 0.000001);
    EXPECT_NEAR(report["simulation"]["finalPosition"].value("x", 0.0), 0.48, 0.000001);
    EXPECT_NEAR(report["simulation"]["finalPosition"].value("y", 0.0), 0.48, 0.000001);
    EXPECT_TRUE(report["diagnostics"].empty());
}

TEST(StarterArenaRuntimeSmokeTests, InvalidPlayableInputSourceFailsClearly)
{
    const auto root = TempProjectRoot("saga_starter_arena_invalid_input_source");
    CopyStarterArenaSample(root);
    const auto reportPath = std::filesystem::temp_directory_path() /
                            "saga_starter_arena_invalid_input_source.json";
    std::filesystem::remove(reportPath);
    const std::string command = ShellQuote(SagaRuntimeExecutable()) + " --project " +
        ShellQuote(root / "StarterArena.sagaproj") +
        " --starter-arena-playable --playable-input-source controller" +
        " --playable-report-out " + ShellQuote(reportPath);
    ASSERT_NE(RunCommand(command), 0);
    const auto report = ReadJson(reportPath);
    EXPECT_EQ(report["input"].value("status", ""), "Failed");
    EXPECT_EQ(report["input"].value("source", ""), "invalid");
    EXPECT_TRUE(ContainsDiagnosticCode(
        report["diagnostics"], "StarterArena.Playable.InputSourceInvalid"));
}

TEST(StarterArenaRuntimeSmokeTests, SyntheticPlayableInputRequiresScript)
{
    const auto root = TempProjectRoot("saga_starter_arena_missing_input_script");
    CopyStarterArenaSample(root);
    const auto reportPath = std::filesystem::temp_directory_path() /
                            "saga_starter_arena_missing_input_script.json";
    std::filesystem::remove(reportPath);
    const std::string command = ShellQuote(SagaRuntimeExecutable()) + " --project " +
        ShellQuote(root / "StarterArena.sagaproj") +
        " --starter-arena-playable --playable-input-source synthetic" +
        " --playable-report-out " + ShellQuote(reportPath);
    ASSERT_NE(RunCommand(command), 0);
    const auto report = ReadJson(reportPath);
    EXPECT_EQ(report["input"].value("status", ""), "Failed");
    EXPECT_EQ(report["input"].value("source", ""), "synthetic");
    EXPECT_TRUE(ContainsDiagnosticCode(
        report["diagnostics"], "StarterArena.Playable.InputScriptRequired"));
}

TEST(StarterArenaRuntimeSmokeTests, MalformedPlayableInputScriptFailsClearly)
{
    const auto root = TempProjectRoot("saga_starter_arena_invalid_input_script");
    CopyStarterArenaSample(root);
    const auto scriptPath = root / "Input" / "invalid.json";
    std::ofstream script(scriptPath);
    script << R"({"schemaVersion":1,"sourceKind":"StarterArenaSyntheticInput","segments":[{"startTick":0,"endTickExclusive":1,"actions":["Jump"]}]})";
    script.close();
    const auto reportPath = std::filesystem::temp_directory_path() /
                            "saga_starter_arena_invalid_input_script.json";
    std::filesystem::remove(reportPath);
    const std::string command = ShellQuote(SagaRuntimeExecutable()) + " --project " +
        ShellQuote(root / "StarterArena.sagaproj") +
        " --starter-arena-playable --playable-input-source synthetic" +
        " --playable-input-script " + ShellQuote(scriptPath) +
        " --playable-report-out " + ShellQuote(reportPath);
    ASSERT_NE(RunCommand(command), 0);
    const auto report = ReadJson(reportPath);
    EXPECT_EQ(report["input"].value("status", ""), "Failed");
    EXPECT_TRUE(ContainsDiagnosticCode(
        report["diagnostics"], "StarterArena.Playable.InputScriptInvalid"));
}
