/// @file ScriptBindingRuntimeTests.cpp
/// @brief Tests for editorless SagaScript binding runtime orchestration.

#include <SagaEngine/Scripting/ScriptBindingRuntime.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace
{

using SagaEngine::Scripting::ISagaScriptHost;
using SagaEngine::Scripting::ISagaScriptWorld;
using SagaEngine::Scripting::ScriptBindingDiagnostics::DuplicateBindingId;
using SagaEngine::Scripting::ScriptBindingDiagnostics::DuplicateCreateEntityKey;
using SagaEngine::Scripting::ScriptBindingDiagnostics::MissingCreateName;
using SagaEngine::Scripting::ScriptBindingDiagnostics::MissingExistingEntityKey;
using SagaEngine::Scripting::ScriptBindingDiagnostics::MissingScriptId;
using SagaEngine::Scripting::ScriptBindingManifestLoader;
using SagaEngine::Scripting::ScriptBindingManifestLoadRequest;
using SagaEngine::Scripting::ScriptBindingRuntime;
using SagaEngine::Scripting::ScriptBindingRuntimeCreateRequest;
using SagaEngine::Scripting::ScriptClassId;
using SagaEngine::Scripting::ScriptDiagnostic;
using SagaEngine::Scripting::ScriptEntityCreateResult;
using SagaEngine::Scripting::ScriptEntityHandle;
using SagaEngine::Scripting::ScriptEntityPositionResult;
using SagaEngine::Scripting::ScriptHostDiagnostics::CapabilityGrantMissing;
using SagaEngine::Scripting::ScriptHostDiagnostics::ClassMissing;
using SagaEngine::Scripting::ScriptHostOperationResult;
using SagaEngine::Scripting::ScriptInstanceCreateRequest;
using SagaEngine::Scripting::ScriptInstanceCreateResult;
using SagaEngine::Scripting::ScriptInstanceHandle;
using SagaEngine::Scripting::ScriptLifecycleInvocation;
using SagaEngine::Scripting::ScriptLifecycleMethod;
using SagaEngine::Scripting::ScriptLifecycleService;
using SagaEngine::Scripting::ScriptPackageHandle;
using SagaEngine::Scripting::ScriptPackageLoadRequest;
using SagaEngine::Scripting::ScriptPackageLoadResult;
using SagaEngine::Scripting::ScriptPackageValidationOptions;
using SagaEngine::Scripting::ScriptVector3;

constexpr const char* kDllArtifactHash =
    "sha256:0343b13708cd52d33ecbb1043b5ae145e73a532bdccf7de6925db5cce6054fd3";

struct CreatedRequest
{
    std::string scriptId;
    std::string classId;
    ScriptEntityHandle self;
};

struct LifecycleCall
{
    ScriptLifecycleMethod method = ScriptLifecycleMethod::OnCreate;
    ScriptInstanceHandle instance;
    double deltaTime = 0.0;
};

[[nodiscard]] ScriptDiagnostic MakeDiagnostic(
    std::string code,
    std::string title,
    std::string message)
{
    ScriptDiagnostic diagnostic;
    diagnostic.diagnostic.code.value = std::move(code);
    diagnostic.diagnostic.title = std::move(title);
    diagnostic.diagnostic.message = std::move(message);
    return diagnostic;
}

class MockHost final : public ISagaScriptHost
{
public:
    std::optional<std::string> failingClass;
    std::size_t loadPackageCallCount = 0;
    std::vector<CreatedRequest> createdRequests;
    std::vector<LifecycleCall> lifecycleCalls;

    [[nodiscard]] ScriptPackageLoadResult LoadPackage(
        const ScriptPackageLoadRequest& request) override
    {
        lastLoadRequest = request;
        ++loadPackageCallCount;
        ScriptPackageLoadResult result;
        result.succeeded = true;
        result.package.value = 7;
        return result;
    }

    [[nodiscard]] ScriptInstanceCreateResult CreateInstance(
        ScriptPackageHandle package,
        const ScriptClassId& classId) override
    {
        ScriptInstanceCreateRequest request;
        request.package = package;
        request.classId = classId;
        return CreateInstance(request);
    }

    [[nodiscard]] ScriptInstanceCreateResult CreateInstance(
        const ScriptInstanceCreateRequest& request) override
    {
        createdRequests.push_back(CreatedRequest{
            request.scriptId,
            request.classId.value,
            request.selfEntity,
        });

        ScriptInstanceCreateResult result;
        if (failingClass.has_value() && request.classId.value == *failingClass)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                ClassMissing,
                "Script class not found",
                "Mock host rejected the requested class."));
            return result;
        }

        result.succeeded = true;
        result.instance.value = nextInstance_++;
        return result;
    }

    [[nodiscard]] ScriptHostOperationResult InvokeLifecycle(
        const ScriptLifecycleInvocation& invocation) override
    {
        lifecycleCalls.push_back(LifecycleCall{
            invocation.method,
            invocation.instance,
            invocation.deltaTimeSeconds,
        });

        ScriptHostOperationResult result;
        result.succeeded = true;
        return result;
    }

    ScriptPackageLoadRequest lastLoadRequest;

private:
    std::uint64_t nextInstance_ = 100;
};

class FakeWorld final : public ISagaScriptWorld
{
public:
    std::vector<std::string> createdNames;

    [[nodiscard]] ScriptEntityCreateResult CreateEntity(
        const std::string& name) override
    {
        createdNames.push_back(name);
        ScriptEntityCreateResult result;
        result.succeeded = true;
        result.entity.value = nextEntity_++;
        return result;
    }

    [[nodiscard]] ScriptHostOperationResult SetPosition(
        ScriptEntityHandle,
        ScriptVector3) override
    {
        ScriptHostOperationResult result;
        result.succeeded = true;
        return result;
    }

    [[nodiscard]] ScriptEntityPositionResult GetPosition(
        ScriptEntityHandle) const override
    {
        ScriptEntityPositionResult result;
        result.succeeded = true;
        return result;
    }

private:
    std::uint64_t nextEntity_ = 500;
};

void WriteFile(const std::filesystem::path& path, const std::string& contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << contents;
}

void WriteJson(const std::filesystem::path& path, const nlohmann::json& value)
{
    WriteFile(path, value.dump(2) + "\n");
}

[[nodiscard]] std::filesystem::path TempRoot(const char* name)
{
    const auto root = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

[[nodiscard]] nlohmann::json Binding(
    const char* bindingId,
    const char* scriptId,
    const char* declaringType)
{
    return {
        {"bindingId", bindingId},
        {"scriptId", scriptId},
        {"declaringType", declaringType},
        {"methodName", "Run"},
        {"authority", "ServerOnly"},
        {"requestedCapabilities", nlohmann::json::array()},
    };
}

[[nodiscard]] nlohmann::json RuntimeBindingExisting(
    const char* bindingId,
    const char* entityKey)
{
    return {
        {"bindingId", bindingId},
        {"self", {
            {"policy", "existing"},
            {"entityKey", entityKey},
        }},
    };
}

[[nodiscard]] nlohmann::json RuntimeBindingCreate(
    const char* bindingId,
    const char* entityKey,
    const char* name)
{
    return {
        {"bindingId", bindingId},
        {"self", {
            {"policy", "create"},
            {"entityKey", entityKey},
            {"name", name},
        }},
    };
}

[[nodiscard]] nlohmann::json Artifact(
    const char* scriptId,
    nlohmann::json requested = nlohmann::json::array(),
    nlohmann::json granted = nlohmann::json::array())
{
    return {
        {"artifactId", std::string("artifact://scripts/") + scriptId},
        {"scriptId", scriptId},
        {"assemblyPath", "Build/Artifacts/Scripts/Game.scripts.dll"},
        {"runtimeConfigPath", "Build/Artifacts/Scripts/Game.scripts.runtimeconfig.json"},
        {"hash", kDllArtifactHash},
        {"authority", "ServerOnly"},
        {"packageDestinationIntent", "server"},
        {"requestedCapabilities", std::move(requested)},
        {"grantedCapabilities", std::move(granted)},
        {"bindingIds", nlohmann::json::array()},
        {"sourceFiles", nlohmann::json::array()},
    };
}

[[nodiscard]] nlohmann::json Capability(
    const char* scriptId,
    nlohmann::json requested = nlohmann::json::array(),
    nlohmann::json granted = nlohmann::json::array())
{
    return {
        {"scriptId", scriptId},
        {"authority", "ServerOnly"},
        {"packageDestinationIntent", "server"},
        {"requestedCapabilities", std::move(requested)},
        {"grantedCapabilities", std::move(granted)},
    };
}

void WriteProject(
    const std::filesystem::path& root,
    nlohmann::json bindings,
    nlohmann::json runtimeBindings,
    nlohmann::json artifacts,
    nlohmann::json capabilities)
{
    WriteJson(
        root / "Build" / "Manifests" / "script_bindings.json",
        {
            {"schemaVersion", 1},
            {"toolName", "sagascript"},
            {"toolchainVersion", "0.0.8-dev"},
            {"sourceHash", "sha256:test"},
            {"bindings", std::move(bindings)},
            {"diagnosticSummary", {{"errorCount", 0}, {"warningCount", 0}, {"hasBlockingDiagnostics", false}}},
        });
    WriteJson(
        root / "Build" / "Manifests" / "script_runtime_bindings.json",
        {
            {"schemaVersion", 1},
            {"bindings", std::move(runtimeBindings)},
        });
    WriteJson(
        root / "Build" / "Manifests" / "script_artifacts.json",
        {
            {"schemaVersion", 1},
            {"toolName", "sagascript"},
            {"toolchainVersion", "0.0.8-dev"},
            {"targetFramework", "net10.0"},
            {"sourceHash", "sha256:test"},
            {"artifacts", std::move(artifacts)},
            {"diagnosticSummary", {{"errorCount", 0}, {"warningCount", 0}, {"hasBlockingDiagnostics", false}}},
        });
    WriteJson(
        root / "Build" / "Manifests" / "script_capabilities.json",
        {
            {"schemaVersion", 1},
            {"toolName", "sagascript"},
            {"toolchainVersion", "0.0.8-dev"},
            {"sourceHash", "sha256:test"},
            {"scripts", std::move(capabilities)},
            {"diagnosticSummary", {{"errorCount", 0}, {"warningCount", 0}, {"hasBlockingDiagnostics", false}}},
        });
    WriteFile(root / "Build" / "Artifacts" / "Scripts" / "Game.scripts.dll", "dll");
    WriteFile(
        root / "Build" / "Artifacts" / "Scripts" / "Game.scripts.runtimeconfig.json",
        R"({"runtimeOptions":{"tfm":"net10.0"}})");
}

[[nodiscard]] ScriptBindingManifestLoadRequest MakeManifestRequest(
    const std::filesystem::path& root)
{
    ScriptBindingManifestLoadRequest request;
    request.packageRoot = root;
    return request;
}

[[nodiscard]] ScriptPackageLoadRequest MakePackageRequest(
    const std::filesystem::path& root)
{
    ScriptPackageLoadRequest request;
    request.packageRoot = root;
    request.scriptArtifactManifest = "Build/Manifests/script_artifacts.json";
    request.packageId = "script.binding.runtime.tests";
    return request;
}

[[nodiscard]] ScriptBindingRuntimeCreateRequest MakeRuntimeRequest(
    const std::filesystem::path& root)
{
    ScriptBindingRuntimeCreateRequest request;
    request.manifestRequest = MakeManifestRequest(root);
    request.packageRequest = MakePackageRequest(root);
    return request;
}

[[nodiscard]] std::string DescribeDiagnostics(
    const std::vector<ScriptDiagnostic>& diagnostics)
{
    std::ostringstream output;
    for (const auto& diagnostic : diagnostics)
    {
        output << diagnostic.diagnostic.code.value << ": "
               << diagnostic.diagnostic.message << "\n";
    }
    return output.str();
}

#if defined(SAGA_SCRIPT_BINDING_RUNTIME_RUNNER)

[[nodiscard]] std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
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
        quoted += character == '"' ? "\\\"" : std::string(1, character);
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

void ConfigureDotnetEnvironment()
{
    SetEnvironment(
        "SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY",
        std::filesystem::path(SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY));
    SetEnvironment("DOTNET_ROOT", std::filesystem::path(SAGA_DOTNET_ROOT));
    SetEnvironment("DOTNET_CLI_HOME", "/tmp/sagascript-dotnet-home-native");
    SetEnvironment("NUGET_PACKAGES", std::filesystem::path(SAGA_NUGET_PACKAGES));
    SetEnvironment("DOTNET_CLI_TELEMETRY_OPTOUT", "1");

    const auto dotnetDirectory =
        std::filesystem::path(SAGA_DOTNET_EXECUTABLE).parent_path().string();
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

[[nodiscard]] std::filesystem::path SagaScriptExecutable()
{
    return std::filesystem::path(SAGA_SOURCE_ROOT) / "Tools" / "SagaScript" /
        "sagascript";
}

[[nodiscard]] std::string RunnerProofSource()
{
    return R"csharp(
namespace Game;

using SagaEngine.Scripting;

[SagaScriptId("script://gameplay/runner-proof")]
public sealed class RunnerProofScript : SagaScript
{
    [BlockCallable]
    [BlockCategory("Gameplay")]
    [BlockName("Run")]
    [ServerOnly]
    public void Run()
    {
    }

    public override void OnStart()
    {
        Context.Log.Info("RunnerProof:Start");
    }
}
)csharp";
}

[[nodiscard]] bool CompileRunnerProject(const std::filesystem::path& root)
{
    WriteFile(root / "Scripts" / "RunnerProof.cs", RunnerProofSource());
    ConfigureDotnetEnvironment();

    const auto stdoutPath = root / "Build" / "Reports" / "sagascript.stdout";
    const auto stderrPath = root / "Build" / "Reports" / "sagascript.stderr";
    const auto diagnostics =
        root / "Build" / "Reports" / "sagascript_diagnostics.json";
    std::filesystem::create_directories(stdoutPath.parent_path());

    const std::string command =
        ShellQuote(SagaScriptExecutable()) +
        " compile --source " + ShellQuote(root / "Scripts") +
        " --out " + ShellQuote(root / "Build" / "Manifests") +
        " --artifacts-out " +
        ShellQuote(root / "Build" / "Artifacts" / "Scripts") +
        " --project-root " + ShellQuote(root) +
        " --diagnostics " + ShellQuote(diagnostics) +
        " --assembly-name RunnerProof --json > " + ShellQuote(stdoutPath) +
        " 2> " + ShellQuote(stderrPath);

    const auto exitCode = RunCommand(command);
    if (exitCode != 0)
    {
        ADD_FAILURE() << "stdout:\n" << ReadFile(stdoutPath)
                      << "\nstderr:\n" << ReadFile(stderrPath)
                      << "\ndiagnostics:\n" << ReadFile(diagnostics);
        return false;
    }
    return true;
}

#endif

} // namespace

TEST(ScriptBindingRuntimeTests, ExistingSelfPolicyCreatesRequestWithSelfEntity)
{
    const auto root = TempRoot("saga_script_binding_runtime_existing_self");
    WriteProject(
        root,
        nlohmann::json::array({Binding("script://game/proof#Game.Player.Run", "script://game/proof", "Game.Player")}),
        nlohmann::json::array({RuntimeBindingExisting("script://game/proof#Game.Player.Run", "player")}),
        nlohmann::json::array({Artifact("script://game/proof")}),
        nlohmann::json::array({Capability("script://game/proof")}));

    MockHost host;
    FakeWorld world;
    ScriptPackageValidationOptions validationOptions;
    validationOptions.expectedPackageDestination = "server";
    ScriptLifecycleService lifecycle(host, {}, validationOptions);
    ScriptBindingRuntime runtime(lifecycle, world);

    auto request = MakeRuntimeRequest(root);
    request.entities.emplace("player", ScriptEntityHandle{42});
    const auto result = runtime.LoadAndCreate(request);

    ASSERT_TRUE(result.Succeeded()) << DescribeDiagnostics(result.diagnostics);
    ASSERT_EQ(host.createdRequests.size(), 1u);
    EXPECT_EQ(host.createdRequests[0].scriptId, "script://game/proof");
    EXPECT_EQ(host.createdRequests[0].classId, "Game.Player");
    EXPECT_EQ(host.createdRequests[0].self.value, 42u);
}

TEST(ScriptBindingRuntimeTests, CreateSelfPolicyCreatesNamedEntityBeforeInstance)
{
    const auto root = TempRoot("saga_script_binding_runtime_create_self");
    WriteProject(
        root,
        nlohmann::json::array({Binding("script://game/spawn#Game.Spawn.Run", "script://game/spawn", "Game.Spawn")}),
        nlohmann::json::array({RuntimeBindingCreate("script://game/spawn#Game.Spawn.Run", "spawned", "spawned")}),
        nlohmann::json::array({Artifact("script://game/spawn")}),
        nlohmann::json::array({Capability("script://game/spawn")}));

    MockHost host;
    FakeWorld world;
    ScriptPackageValidationOptions validationOptions;
    validationOptions.expectedPackageDestination = "server";
    ScriptLifecycleService lifecycle(host, {}, validationOptions);
    ScriptBindingRuntime runtime(lifecycle, world);

    const auto result = runtime.LoadAndCreate(MakeRuntimeRequest(root));

    ASSERT_TRUE(result.Succeeded()) << DescribeDiagnostics(result.diagnostics);
    ASSERT_EQ(world.createdNames.size(), 1u);
    EXPECT_EQ(world.createdNames[0], "spawned");
    ASSERT_EQ(host.createdRequests.size(), 1u);
    EXPECT_EQ(host.createdRequests[0].self.value, 500u);
    EXPECT_EQ(runtime.Entities().at("spawned").value, 500u);
}

TEST(ScriptBindingRuntimeTests, DuplicateGeneratedBindingIdFails)
{
    const auto root = TempRoot("saga_script_binding_runtime_duplicate_binding");
    WriteProject(
        root,
        nlohmann::json::array({
            Binding("script://game/proof#Game.Player.Run", "script://game/proof", "Game.Player"),
            Binding("script://game/proof#Game.Player.Run", "script://game/proof", "Game.Player"),
        }),
        nlohmann::json::array({RuntimeBindingExisting("script://game/proof#Game.Player.Run", "player")}),
        nlohmann::json::array({Artifact("script://game/proof")}),
        nlohmann::json::array({Capability("script://game/proof")}));

    const auto result = ScriptBindingManifestLoader::Load(MakeManifestRequest(root));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, DuplicateBindingId);
}

TEST(ScriptBindingRuntimeTests, MissingGeneratedScriptIdFails)
{
    const auto root = TempRoot("saga_script_binding_runtime_missing_script_id");
    auto binding = Binding("script://game/proof#Game.Player.Run", "script://game/proof", "Game.Player");
    binding["scriptId"] = "";
    WriteProject(
        root,
        nlohmann::json::array({binding}),
        nlohmann::json::array({RuntimeBindingExisting("script://game/proof#Game.Player.Run", "player")}),
        nlohmann::json::array({Artifact("script://game/proof")}),
        nlohmann::json::array({Capability("script://game/proof")}));

    const auto result = ScriptBindingManifestLoader::Load(MakeManifestRequest(root));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, MissingScriptId);
}

TEST(ScriptBindingRuntimeTests, MissingExistingSelfKeyFailsBeforeHostLoad)
{
    const auto root = TempRoot("saga_script_binding_runtime_missing_existing_key");
    WriteProject(
        root,
        nlohmann::json::array({Binding("script://game/proof#Game.Player.Run", "script://game/proof", "Game.Player")}),
        nlohmann::json::array({RuntimeBindingExisting("script://game/proof#Game.Player.Run", "player")}),
        nlohmann::json::array({Artifact("script://game/proof")}),
        nlohmann::json::array({Capability("script://game/proof")}));

    MockHost host;
    FakeWorld world;
    ScriptLifecycleService lifecycle(host);
    ScriptBindingRuntime runtime(lifecycle, world);
    const auto result = runtime.LoadAndCreate(MakeRuntimeRequest(root));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, MissingExistingEntityKey);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptBindingRuntimeTests, DuplicateCreateEntityKeyConflictFails)
{
    const auto root = TempRoot("saga_script_binding_runtime_duplicate_create_key");
    WriteProject(
        root,
        nlohmann::json::array({Binding("script://game/spawn#Game.Spawn.Run", "script://game/spawn", "Game.Spawn")}),
        nlohmann::json::array({RuntimeBindingCreate("script://game/spawn#Game.Spawn.Run", "spawned", "spawned")}),
        nlohmann::json::array({Artifact("script://game/spawn")}),
        nlohmann::json::array({Capability("script://game/spawn")}));

    MockHost host;
    FakeWorld world;
    ScriptLifecycleService lifecycle(host);
    ScriptBindingRuntime runtime(lifecycle, world);
    auto request = MakeRuntimeRequest(root);
    request.entities.emplace("spawned", ScriptEntityHandle{42});
    const auto result = runtime.LoadAndCreate(request);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, DuplicateCreateEntityKey);
    EXPECT_TRUE(world.createdNames.empty());
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptBindingRuntimeTests, MissingCreateNameFailsManifestValidation)
{
    const auto root = TempRoot("saga_script_binding_runtime_missing_create_name");
    WriteProject(
        root,
        nlohmann::json::array({Binding("script://game/spawn#Game.Spawn.Run", "script://game/spawn", "Game.Spawn")}),
        nlohmann::json::array({
            {
                {"bindingId", "script://game/spawn#Game.Spawn.Run"},
                {"self", {{"policy", "create"}, {"entityKey", "spawned"}}},
            },
        }),
        nlohmann::json::array({Artifact("script://game/spawn")}),
        nlohmann::json::array({Capability("script://game/spawn")}));

    const auto result = ScriptBindingManifestLoader::Load(MakeManifestRequest(root));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, MissingCreateName);
}

TEST(ScriptBindingRuntimeTests, CapabilityDenialFailsBeforeHostLoad)
{
    const auto root = TempRoot("saga_script_binding_runtime_capability_denial");
    const auto requested = nlohmann::json::array({"Gameplay.World.CreateEntity"});
    WriteProject(
        root,
        nlohmann::json::array({Binding("script://game/spawn#Game.Spawn.Run", "script://game/spawn", "Game.Spawn")}),
        nlohmann::json::array({RuntimeBindingCreate("script://game/spawn#Game.Spawn.Run", "spawned", "spawned")}),
        nlohmann::json::array({Artifact("script://game/spawn", requested, nlohmann::json::array())}),
        nlohmann::json::array({Capability("script://game/spawn", requested, nlohmann::json::array())}));

    MockHost host;
    FakeWorld world;
    ScriptPackageValidationOptions validationOptions;
    validationOptions.expectedPackageDestination = "server";
    ScriptLifecycleService lifecycle(host, {}, validationOptions);
    ScriptBindingRuntime runtime(lifecycle, world);

    const auto result = runtime.LoadAndCreate(MakeRuntimeRequest(root));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, CapabilityGrantMissing);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptBindingRuntimeTests, LifecycleOrderAndReverseShutdownAreDeterministic)
{
    const auto root = TempRoot("saga_script_binding_runtime_lifecycle_order");
    WriteProject(
        root,
        nlohmann::json::array({
            Binding("script://game/b#Game.B.Run", "script://game/b", "Game.B"),
            Binding("script://game/a#Game.A.Run", "script://game/a", "Game.A"),
        }),
        nlohmann::json::array({
            RuntimeBindingExisting("script://game/b#Game.B.Run", "b"),
            RuntimeBindingExisting("script://game/a#Game.A.Run", "a"),
        }),
        nlohmann::json::array({
            Artifact("script://game/a"),
            Artifact("script://game/b"),
        }),
        nlohmann::json::array({
            Capability("script://game/a"),
            Capability("script://game/b"),
        }));

    MockHost host;
    FakeWorld world;
    ScriptPackageValidationOptions validationOptions;
    validationOptions.expectedPackageDestination = "server";
    ScriptLifecycleService lifecycle(host, {}, validationOptions);
    ScriptBindingRuntime runtime(lifecycle, world);
    auto request = MakeRuntimeRequest(root);
    request.entities.emplace("a", ScriptEntityHandle{1});
    request.entities.emplace("b", ScriptEntityHandle{2});

    ASSERT_TRUE(runtime.LoadAndCreate(request).Succeeded());
    ASSERT_EQ(host.createdRequests.size(), 2u);
    EXPECT_EQ(host.createdRequests[0].classId, "Game.A");
    EXPECT_EQ(host.createdRequests[1].classId, "Game.B");

    ASSERT_TRUE(runtime.StartAll().Succeeded());
    ASSERT_TRUE(runtime.UpdateAll(0.25).Succeeded());
    ASSERT_EQ(host.lifecycleCalls.size(), 6u);
    EXPECT_EQ(host.lifecycleCalls[0].method, ScriptLifecycleMethod::OnCreate);
    EXPECT_EQ(host.lifecycleCalls[0].instance.value, 100u);
    EXPECT_EQ(host.lifecycleCalls[1].method, ScriptLifecycleMethod::OnStart);
    EXPECT_EQ(host.lifecycleCalls[1].instance.value, 100u);
    EXPECT_EQ(host.lifecycleCalls[2].method, ScriptLifecycleMethod::OnCreate);
    EXPECT_EQ(host.lifecycleCalls[2].instance.value, 101u);
    EXPECT_EQ(host.lifecycleCalls[3].method, ScriptLifecycleMethod::OnStart);
    EXPECT_EQ(host.lifecycleCalls[3].instance.value, 101u);
    EXPECT_EQ(host.lifecycleCalls[4].method, ScriptLifecycleMethod::OnUpdate);
    EXPECT_EQ(host.lifecycleCalls[4].instance.value, 100u);
    EXPECT_DOUBLE_EQ(host.lifecycleCalls[4].deltaTime, 0.25);
    EXPECT_EQ(host.lifecycleCalls[5].method, ScriptLifecycleMethod::OnUpdate);
    EXPECT_EQ(host.lifecycleCalls[5].instance.value, 101u);

    ASSERT_TRUE(runtime.Shutdown().Succeeded());
    ASSERT_EQ(host.lifecycleCalls.size(), 8u);
    EXPECT_EQ(host.lifecycleCalls[6].method, ScriptLifecycleMethod::OnDestroy);
    EXPECT_EQ(host.lifecycleCalls[6].instance.value, 101u);
    EXPECT_EQ(host.lifecycleCalls[7].method, ScriptLifecycleMethod::OnDestroy);
    EXPECT_EQ(host.lifecycleCalls[7].instance.value, 100u);
}

TEST(ScriptBindingRuntimeTests, PartialInstanceCreationFailureDestroysCreatedInstances)
{
    const auto root = TempRoot("saga_script_binding_runtime_partial_failure");
    WriteProject(
        root,
        nlohmann::json::array({
            Binding("script://game/a#Game.A.Run", "script://game/a", "Game.A"),
            Binding("script://game/b#Game.B.Run", "script://game/b", "Game.B"),
        }),
        nlohmann::json::array({
            RuntimeBindingExisting("script://game/a#Game.A.Run", "a"),
            RuntimeBindingExisting("script://game/b#Game.B.Run", "b"),
        }),
        nlohmann::json::array({
            Artifact("script://game/a"),
            Artifact("script://game/b"),
        }),
        nlohmann::json::array({
            Capability("script://game/a"),
            Capability("script://game/b"),
        }));

    MockHost host;
    host.failingClass = "Game.B";
    FakeWorld world;
    ScriptPackageValidationOptions validationOptions;
    validationOptions.expectedPackageDestination = "server";
    ScriptLifecycleService lifecycle(host, {}, validationOptions);
    ScriptBindingRuntime runtime(lifecycle, world);
    auto request = MakeRuntimeRequest(root);
    request.entities.emplace("a", ScriptEntityHandle{1});
    request.entities.emplace("b", ScriptEntityHandle{2});

    const auto result = runtime.LoadAndCreate(request);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(host.lifecycleCalls.size(), 1u);
    EXPECT_EQ(host.lifecycleCalls[0].method, ScriptLifecycleMethod::OnDestroy);
    EXPECT_EQ(host.lifecycleCalls[0].instance.value, 100u);
    EXPECT_TRUE(runtime.Instances().empty());
}

#if defined(SAGA_SCRIPT_BINDING_RUNTIME_RUNNER)

TEST(ScriptBindingRuntimeTests, ThinRunnerSucceedsForMinimalCompiledProject)
{
    const auto root = TempRoot("saga_script_binding_runtime_runner_smoke");
    std::filesystem::create_directories(root / "Scripts");
    ASSERT_TRUE(CompileRunnerProject(root));
    WriteJson(
        root / "Build" / "Manifests" / "script_runtime_bindings.json",
        {
            {"schemaVersion", 1},
            {"bindings", nlohmann::json::array({
                RuntimeBindingCreate(
                    "script://gameplay/runner-proof#Game.RunnerProofScript.Run",
                    "runner",
                    "runner"),
            })},
        });

    const auto stdoutPath = root / "Build" / "Reports" / "runner.stdout";
    const auto stderrPath = root / "Build" / "Reports" / "runner.stderr";
    const std::string command =
        ShellQuote(std::filesystem::path(SAGA_SCRIPT_BINDING_RUNTIME_RUNNER)) +
        " --project " + ShellQuote(root) + " --run > " +
        ShellQuote(stdoutPath) + " 2> " + ShellQuote(stderrPath);

    const auto exitCode = RunCommand(command);
    EXPECT_EQ(exitCode, 0) << "stdout:\n" << ReadFile(stdoutPath)
                           << "\nstderr:\n" << ReadFile(stderrPath);
}

#endif
