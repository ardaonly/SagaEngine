/// @file CSharpGameplayProofTests.cpp
/// @brief Editorless proof tests for CLI-compiled C# gameplay scripts.

#include <SagaEngine/ECS/ComponentRegistry.h>
#include <SagaEngine/ECS/Components/TransformComponent.h>
#include <SagaEngine/Scripting/CSharpScriptHost.hpp>
#include <SagaEngine/Scripting/ScriptLifecycleService.hpp>
#include <SagaEngine/Scripting/WorldStateScriptWorld.hpp>
#include <SagaEngine/Simulation/WorldState.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace
{

using SagaEngine::ECS::ComponentRegistry;
using SagaEngine::ECS::TransformComponent;
using SagaEngine::Scripting::CSharpScriptHost;
using SagaEngine::Scripting::CSharpScriptHostOptions;
using SagaEngine::Scripting::ScriptClassId;
using SagaEngine::Scripting::ScriptEntityHandle;
using SagaEngine::Scripting::ScriptHostDiagnostics::CapabilityGrantMissing;
using SagaEngine::Scripting::ScriptInstanceCreateRequest;
using SagaEngine::Scripting::ScriptLifecycleService;
using SagaEngine::Scripting::ScriptLogEvent;
using SagaEngine::Scripting::ScriptPackageLoadRequest;
using SagaEngine::Scripting::ScriptPackageValidationOptions;
using SagaEngine::Scripting::WorldStateScriptWorld;
using SagaEngine::Simulation::WorldState;

constexpr const char* kCreateEntityCapability = "Gameplay.World.CreateEntity";
constexpr const char* kTransformReadCapability = "Gameplay.Transform.Read";
constexpr const char* kTransformWriteCapability = "Gameplay.Transform.Write";
constexpr const char* kSelfScriptId = "script://gameplay/editorless-proof";
constexpr const char* kCreateScriptId =
    "script://gameplay/editorless-create-entity-proof";

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

[[nodiscard]] std::filesystem::path TempProjectRoot(const char* name)
{
    const auto root = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "Scripts");
    return root;
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

[[nodiscard]] std::filesystem::path SagaScriptExecutable()
{
    return std::filesystem::path(SAGA_SOURCE_ROOT) / "Tools" / "SagaScript" /
        "sagascript";
}

[[nodiscard]] std::filesystem::path RuntimeBridgeAssembly()
{
    return std::filesystem::path(SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY);
}

[[nodiscard]] std::filesystem::path DotnetExecutable()
{
    return std::filesystem::path(SAGA_DOTNET_EXECUTABLE);
}

[[nodiscard]] CSharpScriptHostOptions HostOptions(
    WorldStateScriptWorld& world,
    std::vector<ScriptLogEvent>& logs)
{
    CSharpScriptHostOptions options;
    options.runtimeBridgeAssembly = RuntimeBridgeAssembly();
    options.dotnetRoot = std::filesystem::path(SAGA_DOTNET_ROOT);
    options.world = &world;
    options.logSink = [&logs](const ScriptLogEvent& event)
    {
        logs.push_back(event);
    };
    return options;
}

[[nodiscard]] ScriptPackageLoadRequest MakePackageRequest(
    const std::filesystem::path& root)
{
    ScriptPackageLoadRequest request;
    request.packageRoot = root;
    request.scriptArtifactManifest = "Build/Manifests/script_artifacts.json";
    request.packageId = "editorless.gameplay.proof";
    return request;
}

[[nodiscard]] std::string DescribeDiagnostics(
    const std::vector<SagaEngine::Scripting::ScriptDiagnostic>& diagnostics)
{
    std::ostringstream output;
    for (const auto& diagnostic : diagnostics)
    {
        output << diagnostic.diagnostic.code.value << ": "
               << diagnostic.diagnostic.message;
        for (const auto& [key, value] : diagnostic.metadata)
        {
            output << " " << key << "=" << value;
        }
        output << "\n";
    }
    return output.str();
}

[[nodiscard]] nlohmann::json JsonStringArray(
    const std::vector<std::string>& values)
{
    nlohmann::json array = nlohmann::json::array();
    for (const auto& value : values)
    {
        array.push_back(value);
    }
    return array;
}

void WriteJson(const std::filesystem::path& path, const nlohmann::json& value)
{
    std::ofstream output(path);
    output << value.dump(2) << "\n";
}

[[nodiscard]] nlohmann::json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path);
    nlohmann::json value;
    input >> value;
    return value;
}

void SetGrantedCapabilities(
    const std::filesystem::path& root,
    const std::vector<std::string>& grants)
{
    const auto granted = JsonStringArray(grants);

    const auto artifactsPath =
        root / "Build" / "Manifests" / "script_artifacts.json";
    auto artifacts = ReadJson(artifactsPath);
    for (auto& artifact : artifacts["artifacts"])
    {
        artifact["grantedCapabilities"] = granted;
    }
    WriteJson(artifactsPath, artifacts);

    const auto capabilitiesPath =
        root / "Build" / "Manifests" / "script_capabilities.json";
    auto capabilities = ReadJson(capabilitiesPath);
    for (auto& script : capabilities["scripts"])
    {
        script["grantedCapabilities"] = granted;
    }
    WriteJson(capabilitiesPath, capabilities);
}

void ConfigureDotnetEnvironment()
{
    SetEnvironment("SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY", RuntimeBridgeAssembly());
    SetEnvironment("DOTNET_ROOT", std::filesystem::path(SAGA_DOTNET_ROOT));
    SetEnvironment("DOTNET_CLI_HOME", "/tmp/sagascript-dotnet-home-native");
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

[[nodiscard]] bool CompileProjectSources(
    const std::filesystem::path& root,
    const char* assemblyName)
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
        " --assembly-name " + ShellQuote(std::string_view(assemblyName)) +
        " --json > " + ShellQuote(stdoutPath) +
        " 2> " + ShellQuote(stderrPath);

    const auto exitCode = RunCommand(command);
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

[[nodiscard]] bool CompileProject(
    const std::filesystem::path& root,
    const std::string& source,
    const char* assemblyName)
{
    WriteFile(root / "Scripts" / "GameplayProof.cs", source);
    return CompileProjectSources(root, assemblyName);
}

[[nodiscard]] std::filesystem::path StarterArenaRoot()
{
    return std::filesystem::path(SAGA_SOURCE_ROOT) / "Samples" /
        "StarterArena";
}

[[nodiscard]] std::filesystem::path StarterArenaProjectPath()
{
    return StarterArenaRoot() / "StarterArena.sagaproj";
}

[[nodiscard]] std::filesystem::path StarterArenaGameRulesPath()
{
    return StarterArenaRoot() / "Scripts" / "GameRules.cs";
}

[[nodiscard]] bool StarterArenaDeclaresScriptsFolder()
{
    const auto manifest = ReadJson(StarterArenaProjectPath());
    const auto scripts = manifest.value("scriptFolders", nlohmann::json::array());
    return std::any_of(
        scripts.begin(),
        scripts.end(),
        [](const nlohmann::json& folder)
        {
            return folder.value("path", "") == "Scripts";
        });
}

[[nodiscard]] bool CompileStarterArenaProject(
    const std::filesystem::path& root)
{
    std::filesystem::create_directories(root / "Scripts");
    std::filesystem::copy_file(
        StarterArenaGameRulesPath(),
        root / "Scripts" / "GameRules.cs",
        std::filesystem::copy_options::overwrite_existing);
    return CompileProjectSources(root, "StarterArenaScripts");
}

void EnsureTransformRegistered()
{
    auto& registry = ComponentRegistry::Instance();
    if (!registry.IsRegistered<TransformComponent>())
    {
        registry.Register<TransformComponent>(1, "TransformComponent");
    }
}

[[nodiscard]] ScriptInstanceCreateRequest MakeInstanceRequest(
    const SagaEngine::Scripting::ScriptPackageHandle package,
    const char* scriptId,
    const char* classId,
    const ScriptEntityHandle self = {})
{
    ScriptInstanceCreateRequest request;
    request.package = package;
    request.scriptId = scriptId;
    request.classId = ScriptClassId{classId};
    request.selfEntity = self;
    return request;
}

[[nodiscard]] bool HasLog(
    const std::vector<ScriptLogEvent>& logs,
    std::string_view message)
{
    return std::any_of(
        logs.begin(),
        logs.end(),
        [message](const ScriptLogEvent& event)
        {
            return event.message == message;
        });
}

[[nodiscard]] std::string SelfMovementSource()
{
    return R"csharp(
namespace Game;

using SagaEngine.Scripting;

[SagaScriptId("script://gameplay/editorless-proof")]
public sealed class EditorlessSelfMovementScript : SagaScript
{
    [BlockCallable]
    [BlockCategory("Gameplay")]
    [BlockName("Move Self")]
    [ServerOnly]
    [RequiresCapability("Gameplay.Transform.Read")]
    [RequiresCapability("Gameplay.Transform.Write")]
    public void MoveSelfBlock()
    {
    }

    public override void OnCreate()
    {
        Context.Log.Info("SelfProof:Create");
    }

    public override void OnStart()
    {
        Context.Self.TrySetPosition(1.0F, 2.0F, 3.0F);
        Context.Log.Info("SelfProof:Start");
    }

    public override void OnUpdate(float deltaTime)
    {
        if (!Context.Self.TryGetPosition(out var position))
        {
            Context.Log.Error("SelfProof:ReadFailed");
            return;
        }

        Context.Self.TrySetPosition(
            position.X + deltaTime * 2.0F,
            position.Y + 1.0F,
            position.Z - deltaTime);
        Context.Log.Info("SelfProof:Update:" + Format(deltaTime));
    }

    private static string Format(float value)
    {
        return value.ToString("R", System.Globalization.CultureInfo.InvariantCulture);
    }
}
)csharp";
}

[[nodiscard]] std::string CreateEntitySource()
{
    return R"csharp(
namespace Game;

using SagaEngine.Scripting;

[SagaScriptId("script://gameplay/editorless-create-entity-proof")]
public sealed class EditorlessCreateEntityScript : SagaScript
{
    [BlockCallable]
    [BlockCategory("Gameplay")]
    [BlockName("Create Entity")]
    [ServerOnly]
    [RequiresCapability("Gameplay.World.CreateEntity")]
    [RequiresCapability("Gameplay.Transform.Read")]
    [RequiresCapability("Gameplay.Transform.Write")]
    public void CreateEntityBlock()
    {
    }

    public override void OnStart()
    {
        if (!Context.World.TryCreateEntity("proof-spawn", out var entity))
        {
            Context.Log.Error("CreateProof:CreateFailed");
            return;
        }

        entity.TrySetPosition(7.0F, 8.0F, 9.0F);
        if (entity.TryGetPosition(out var position))
        {
            Context.Log.Info("CreateProof:Position:" + Format(position.X));
        }
    }

    private static string Format(float value)
    {
        return value.ToString("R", System.Globalization.CultureInfo.InvariantCulture);
    }
}
)csharp";
}

} // namespace

TEST(CSharpGameplayProofTests, EditorlessCompiledScriptMutatesSelfPosition)
{
    EnsureTransformRegistered();
    const auto root = TempProjectRoot("saga_csharp_gameplay_self_proof");
    ASSERT_TRUE(CompileProject(root, SelfMovementSource(), "SelfGameplayProof"));
    SetGrantedCapabilities(
        root,
        {kTransformReadCapability, kTransformWriteCapability});

    WorldState worldState;
    WorldStateScriptWorld scriptWorld(worldState);
    const auto self = scriptWorld.CreateEntity("self").entity;
    std::vector<ScriptLogEvent> logs;

    CSharpScriptHost host(HostOptions(scriptWorld, logs));
    ScriptPackageValidationOptions validationOptions;
    validationOptions.expectedPackageDestination = "server";
    ScriptLifecycleService service(host, {}, validationOptions);

    const auto load = service.LoadPackage(MakePackageRequest(root));
    ASSERT_TRUE(load.Succeeded()) << DescribeDiagnostics(load.diagnostics);
    const auto instance = service.CreateInstance(MakeInstanceRequest(
        load.package,
        kSelfScriptId,
        "Game.EditorlessSelfMovementScript",
        self));
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    ASSERT_TRUE(service.StartInstance(instance.instance).Succeeded());
    ASSERT_TRUE(service.UpdateInstance(instance.instance, 0.25).Succeeded());
    ASSERT_TRUE(service.UpdateInstance(instance.instance, 0.75).Succeeded());

    const auto position = scriptWorld.GetPosition(self);
    ASSERT_TRUE(position.Succeeded()) << DescribeDiagnostics(position.diagnostics);
    EXPECT_FLOAT_EQ(position.position.x, 3.0F);
    EXPECT_FLOAT_EQ(position.position.y, 4.0F);
    EXPECT_FLOAT_EQ(position.position.z, 2.0F);
    EXPECT_TRUE(HasLog(logs, "SelfProof:Create"));
    EXPECT_TRUE(HasLog(logs, "SelfProof:Start"));
    EXPECT_TRUE(HasLog(logs, "SelfProof:Update:0.25"));
    EXPECT_TRUE(HasLog(logs, "SelfProof:Update:0.75"));
}

TEST(CSharpGameplayProofTests, StarterArenaGameRulesRunsCompiledLifecycleHeadlessly)
{
    ASSERT_TRUE(std::filesystem::exists(StarterArenaProjectPath()));
    ASSERT_TRUE(std::filesystem::exists(StarterArenaGameRulesPath()));
    ASSERT_TRUE(StarterArenaDeclaresScriptsFolder());

    const auto source = ReadFile(StarterArenaGameRulesPath());
    ASSERT_NE(
        source.find("[SagaScriptId(\"script://starter-arena/game-rules\")]"),
        std::string::npos);
    ASSERT_NE(source.find("[SharedPure]"), std::string::npos);
    ASSERT_NE(source.find("AddPickupScore"), std::string::npos);

    EnsureTransformRegistered();
    const auto root = TempProjectRoot("saga_starter_arena_lifecycle_proof");
    ASSERT_TRUE(CompileStarterArenaProject(root));

    WorldState worldState;
    WorldStateScriptWorld scriptWorld(worldState);
    std::vector<ScriptLogEvent> logs;

    CSharpScriptHost host(HostOptions(scriptWorld, logs));
    ScriptPackageValidationOptions validationOptions;
    validationOptions.expectedPackageDestination = "server";
    ScriptLifecycleService service(host, {}, validationOptions);

    const auto load = service.LoadPackage(MakePackageRequest(root));
    ASSERT_TRUE(load.Succeeded()) << DescribeDiagnostics(load.diagnostics);
    const auto instance = service.CreateInstance(MakeInstanceRequest(
        load.package,
        "script://starter-arena/game-rules",
        "StarterArena.Scripts.GameRules"));
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    ASSERT_TRUE(service.StartInstance(instance.instance).Succeeded());
    ASSERT_TRUE(service.UpdateInstance(instance.instance, 0.016).Succeeded());
    ASSERT_TRUE(service.DestroyInstance(instance.instance).Succeeded());

    EXPECT_TRUE(HasLog(logs, "StarterArena.GameRules.OnCreate"));
    EXPECT_TRUE(HasLog(logs, "StarterArena.GameRules.OnStart"));
    EXPECT_TRUE(HasLog(logs, "StarterArena.GameRules.OnUpdate"));
    EXPECT_TRUE(HasLog(logs, "StarterArena.GameRules.OnDestroy"));
    EXPECT_EQ(worldState.EntityCount(), 0u);
}

TEST(CSharpGameplayProofTests, EditorlessCompiledScriptCanCreateEntityWhenGranted)
{
    EnsureTransformRegistered();
    const auto root = TempProjectRoot("saga_csharp_gameplay_create_proof");
    ASSERT_TRUE(CompileProject(root, CreateEntitySource(), "CreateGameplayProof"));
    SetGrantedCapabilities(
        root,
        {kCreateEntityCapability, kTransformReadCapability, kTransformWriteCapability});

    WorldState worldState;
    WorldStateScriptWorld scriptWorld(worldState);
    std::vector<ScriptLogEvent> logs;

    CSharpScriptHost host(HostOptions(scriptWorld, logs));
    ScriptPackageValidationOptions validationOptions;
    validationOptions.expectedPackageDestination = "server";
    ScriptLifecycleService service(host, {}, validationOptions);

    const auto load = service.LoadPackage(MakePackageRequest(root));
    ASSERT_TRUE(load.Succeeded()) << DescribeDiagnostics(load.diagnostics);
    const auto instance = service.CreateInstance(MakeInstanceRequest(
        load.package,
        kCreateScriptId,
        "Game.EditorlessCreateEntityScript"));
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    ASSERT_TRUE(service.StartInstance(instance.instance).Succeeded());

    const auto transformType = ComponentRegistry::Instance().GetId<TransformComponent>();
    const auto entities = worldState.Query({transformType});
    ASSERT_EQ(entities.size(), 1u);
    const auto* transform = worldState.GetComponent<TransformComponent>(entities[0]);
    ASSERT_NE(transform, nullptr);
    EXPECT_FLOAT_EQ(transform->transform.position.x, 7.0F);
    EXPECT_FLOAT_EQ(transform->transform.position.y, 8.0F);
    EXPECT_FLOAT_EQ(transform->transform.position.z, 9.0F);
    EXPECT_TRUE(HasLog(logs, "CreateProof:Position:7"));
}

TEST(CSharpGameplayProofTests, EditorlessCompiledScriptCreateEntityDeniedWithoutGrant)
{
    EnsureTransformRegistered();
    const auto root = TempProjectRoot("saga_csharp_gameplay_denied_proof");
    ASSERT_TRUE(CompileProject(root, CreateEntitySource(), "DeniedGameplayProof"));
    SetGrantedCapabilities(
        root,
        {kTransformReadCapability, kTransformWriteCapability});

    WorldState worldState;
    WorldStateScriptWorld scriptWorld(worldState);
    std::vector<ScriptLogEvent> logs;

    CSharpScriptHost host(HostOptions(scriptWorld, logs));
    ScriptPackageValidationOptions validationOptions;
    validationOptions.expectedPackageDestination = "server";
    ScriptLifecycleService service(host, {}, validationOptions);

    const auto load = service.LoadPackage(MakePackageRequest(root));

    ASSERT_FALSE(load.Succeeded());
    ASSERT_EQ(load.diagnostics.size(), 1u) << DescribeDiagnostics(load.diagnostics);
    EXPECT_EQ(load.diagnostics[0].diagnostic.code.value, CapabilityGrantMissing);
    EXPECT_EQ(load.diagnostics[0].metadata.at("capability"), kCreateEntityCapability);
    EXPECT_EQ(worldState.EntityCount(), 0u);
    EXPECT_TRUE(logs.empty());
}
