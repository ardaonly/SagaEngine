// SPDX-License-Identifier: MPL-2.0
/// @file CSharpScriptHostTests.cpp
/// @brief Focused tests for the hostfxr-backed C# SagaScript host.

#include <SagaEngine/Scripting/CSharpScriptHost.hpp>
#include <SagaEngine/Scripting/ScriptLifecycleService.hpp>
#include <SagaEngine/UI/CSharpUiNamedActionHandler.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <cstdlib>
#else
#include <cstdlib>
#endif

namespace
{

using SagaEngine::Scripting::CSharpScriptHost;
using SagaEngine::Scripting::CSharpScriptHostOptions;
using SagaEngine::Scripting::ISagaScriptWorld;
using SagaEngine::Scripting::ScriptClassId;
using SagaEngine::Scripting::ScriptEntityCreateResult;
using SagaEngine::Scripting::ScriptEntityHandle;
using SagaEngine::Scripting::ScriptEntityPositionResult;
using SagaEngine::Scripting::ScriptHostOperationResult;
using SagaEngine::Scripting::ScriptHostDiagnostics::AssemblyLoadFailed;
using SagaEngine::Scripting::ScriptHostDiagnostics::ClassInvalid;
using SagaEngine::Scripting::ScriptHostDiagnostics::ClassMissing;
using SagaEngine::Scripting::ScriptHostDiagnostics::InvalidEntityHandle;
using SagaEngine::Scripting::ScriptHostDiagnostics::InvalidInstanceHandle;
using SagaEngine::Scripting::ScriptHostDiagnostics::InstanceCreationFailed;
using SagaEngine::Scripting::ScriptHostDiagnostics::ManagedException;
using SagaEngine::Scripting::ScriptHostDiagnostics::RuntimeConfigInvalid;
using SagaEngine::Scripting::ScriptHostDiagnostics::RuntimeConfigMissing;
using SagaEngine::Scripting::ScriptHostDiagnostics::ScriptCapabilityDenied;
using SagaEngine::Scripting::ScriptHostDiagnostics::UiNamedActionInvalidSignature;
using SagaEngine::Scripting::ScriptHostDiagnostics::UiNamedActionMethodMissing;
using SagaEngine::Scripting::ScriptHostDiagnostics::UiNamedActionReturnedFalse;
using SagaEngine::Scripting::ScriptInstanceCreateRequest;
using SagaEngine::Scripting::ScriptInstanceHandle;
using SagaEngine::Scripting::ScriptLifecycleService;
using SagaEngine::Scripting::ScriptLogEvent;
using SagaEngine::Scripting::ScriptPackageLoadRequest;
using SagaEngine::Scripting::ScriptUiNamedActionContext;
using SagaEngine::Scripting::ScriptUiNamedActionEventType;
using SagaEngine::Scripting::ScriptUiNamedActionInvocation;
using SagaEngine::Scripting::ScriptVector3;
using SagaEngine::Scripting::VerifiedScriptFile;
using SagaEngine::Scripting::VerifiedScriptGenerationLoadRequest;
using SagaEngine::UI::CSharpUiNamedActionHandler;
using SagaEngine::UI::CSharpUiNamedActionHandlerOptions;
using SagaEngine::UI::UiActionId;
using SagaEngine::UI::UiDiagnosticSeverity;
using SagaEngine::UI::UiElementId;
using SagaEngine::UI::UiEventType;
using SagaEngine::UI::UiNamedActionContext;
using SagaEngine::UI::UiNamedActionHandlerDiagnosticCode;
using SagaEngine::UI::UiScreenId;

constexpr const char* kCreateEntityCapability = "Gameplay.World.CreateEntity";
constexpr const char* kTransformReadCapability = "Gameplay.Transform.Read";
constexpr const char* kTransformWriteCapability = "Gameplay.Transform.Write";

[[nodiscard]] std::filesystem::path TempPackageRoot(const char* name)
{
    const auto root = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

void WriteFile(const std::filesystem::path& path, const std::string& contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << contents;
}

[[nodiscard]] std::string Sha256Hex(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    using EvpContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    EvpContext context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    EXPECT_TRUE(context != nullptr);
    EXPECT_EQ(EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr), 1);

    std::array<char, 8192> buffer{};
    while (input.good())
    {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0)
        {
            EXPECT_EQ(EVP_DigestUpdate(
                          context.get(),
                          buffer.data(),
                          static_cast<std::size_t>(count)),
                      1);
        }
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digestLength = 0;
    EXPECT_EQ(EVP_DigestFinal_ex(context.get(), digest.data(), &digestLength), 1);

    constexpr char kHex[] = "0123456789abcdef";
    std::string hex;
    for (unsigned int index = 0; index < digestLength; ++index)
    {
        const auto byte = digest[index];
        hex.push_back(kHex[(byte >> 4) & 0x0F]);
        hex.push_back(kHex[byte & 0x0F]);
    }
    return hex;
}

[[nodiscard]] std::filesystem::path FixturePath(const char* fileName);
[[nodiscard]] std::filesystem::path RuntimeBridgeAssembly();

[[nodiscard]] VerifiedScriptFile VerifiedFile(const std::filesystem::path& path)
{
    return {path, "sha256:" + Sha256Hex(path)};
}

struct VerifiedGenerationFixture
{
    std::filesystem::path root;
    VerifiedScriptGenerationLoadRequest request;
};

[[nodiscard]] VerifiedGenerationFixture CreateVerifiedGeneration(const char* name)
{
    VerifiedGenerationFixture fixture;
    fixture.root = TempPackageRoot(name);
    const auto assembly = fixture.root / "fixture.scripts.dll";
    const auto runtimeConfig = fixture.root / "fixture.scripts.runtimeconfig.json";
    std::filesystem::copy_file(FixturePath("CSharpScriptHostFixture.scripts.dll"),
        assembly, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(
        FixturePath("CSharpScriptHostFixture.scripts.runtimeconfig.json"),
        runtimeConfig, std::filesystem::copy_options::overwrite_existing);

    auto& request = fixture.request;
    request.projectId = "project-fixture";
    request.projectHash = "sha256:project";
    request.sourceRevision = "sha256:revision";
    request.sourceSetHash = "sha256:sources";
    request.referenceManifestHash = "sha256:references";
    request.packageGraphHash = "sha256:packages";
    request.runtimeBridgeLogicalReferenceId = "runtime-bridge";
    request.runtimeBridgeHash = "sha256:" + Sha256Hex(RuntimeBridgeAssembly());
    request.runtimeBridgeAbi = "NativeCallbackTable.v2+NamedAction.v1";
    request.targetFramework = "net10.0";
    request.runtimeCompatibility = "0.0.11";
    request.trust = "TrustedEngineDistribution";
    request.assembly = VerifiedFile(assembly);
    request.runtimeConfig = VerifiedFile(runtimeConfig);

    const auto identity = [&]
    {
        return nlohmann::json{{"schemaVersion", 1}, {"projectId", request.projectId},
            {"projectHash", request.projectHash},
            {"sourceRevision", request.sourceRevision},
            {"sourceSetHash", request.sourceSetHash}};
    };
    auto assemblyManifest = identity();
    assemblyManifest.update({{"assemblyHash", request.assembly.byteHash},
        {"runtimeConfigHash", request.runtimeConfig.byteHash},
        {"referenceManifestHash", request.referenceManifestHash},
        {"runtimeBridgeLogicalReferenceId", request.runtimeBridgeLogicalReferenceId},
        {"runtimeBridgeHash", request.runtimeBridgeHash},
        {"targetFramework", request.targetFramework},
        {"runtimeCompatibility", request.runtimeCompatibility}, {"trust", request.trust}});
    auto behaviorManifest = identity();
    behaviorManifest["behaviors"] = nlohmann::json::array({
        {{"behaviorId", "Game.PlayerScript"}, {"capabilities", nlohmann::json::array({
            kTransformReadCapability})}, {"actions", nlohmann::json::array({
                {{"actionId", "Interact"}, {"attributeTypeId",
                    "SagaEngine.Scripting.SagaActionAttribute"}, {"method", "OnStart"},
                    {"publiclyInvocable", true}}})}}});
    auto actionManifest = identity();
    actionManifest["actions"] = nlohmann::json::array({
        {{"behaviorId", "Game.PlayerScript"}, {"actionId", "Interact"},
            {"attributeTypeId", "SagaEngine.Scripting.SagaActionAttribute"},
            {"method", "OnStart"}, {"publiclyInvocable", true}}});
    auto capabilityManifest = identity();
    capabilityManifest["packageGraphHash"] = request.packageGraphHash;
    capabilityManifest["capabilities"] = nlohmann::json::array({kTransformReadCapability});

    const auto writeManifest = [&](const char* fileName, const nlohmann::json& value)
    {
        const auto path = fixture.root / fileName;
        WriteFile(path, value.dump(2) + "\n");
        return VerifiedFile(path);
    };
    request.assemblyManifest = writeManifest("assembly_manifest_v1.json", assemblyManifest);
    request.behaviorManifest = writeManifest("behavior_manifest_v1.json", behaviorManifest);
    request.namedActionManifest = writeManifest("named_actions_v1.json", actionManifest);
    request.capabilityManifest = writeManifest("capabilities_v1.json", capabilityManifest);
    return fixture;
}

void RewriteVerifiedJson(VerifiedScriptFile& file, const nlohmann::json& value)
{
    WriteFile(file.path, value.dump(2) + "\n");
    file.byteHash = "sha256:" + Sha256Hex(file.path);
}

[[nodiscard]] nlohmann::json ReadJson(const VerifiedScriptFile& file)
{
    std::ifstream input(file.path);
    return nlohmann::json::parse(input);
}

[[nodiscard]] ScriptPackageLoadRequest MakePackageRequest(
    const std::filesystem::path& root)
{
    ScriptPackageLoadRequest request;
    request.packageRoot = root;
    request.scriptArtifactManifest = "Manifests/script_artifacts.json";
    request.packageId = "csharp.fixture";
    return request;
}

[[nodiscard]] std::filesystem::path FixturePath(const char* fileName)
{
    return std::filesystem::path(SAGA_CSHARP_SCRIPT_FIXTURE_DIR) / fileName;
}

[[nodiscard]] std::filesystem::path RuntimeBridgeAssembly()
{
    return std::filesystem::path(SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY);
}

[[nodiscard]] CSharpScriptHostOptions HostOptions(
    ISagaScriptWorld* world = nullptr,
    std::vector<ScriptLogEvent>* logEvents = nullptr)
{
    CSharpScriptHostOptions options;
    options.runtimeBridgeAssembly = RuntimeBridgeAssembly();
    options.dotnetRoot = std::filesystem::path(SAGA_DOTNET_ROOT);
    options.world = world;
    if (logEvents != nullptr)
    {
        options.logSink = [logEvents](const ScriptLogEvent& event)
        {
            logEvents->push_back(event);
        };
    }
    return options;
}

void SetEnvironment(const char* name, const std::filesystem::path& value)
{
#if defined(_WIN32)
    _putenv_s(name, value.string().c_str());
#else
    setenv(name, value.string().c_str(), 1);
#endif
}

[[nodiscard]] std::string JsonStringArray(
    const std::vector<std::string>& values)
{
    std::ostringstream output;
    output << "[";
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index != 0)
        {
            output << ", ";
        }
        output << "\"" << values[index] << "\"";
    }
    output << "]";
    return output.str();
}

[[nodiscard]] std::filesystem::path CreateValidPackage(
    const char* name,
    const std::vector<std::string>& capabilities = {})
{
    const auto root = TempPackageRoot(name);
    const auto artifactDirectory = root / "Build" / "Artifacts" / "Scripts";
    std::filesystem::create_directories(artifactDirectory);

    const auto assemblyPath = artifactDirectory /
        "CSharpScriptHostFixture.scripts.dll";
    const auto runtimeConfigPath = artifactDirectory /
        "CSharpScriptHostFixture.scripts.runtimeconfig.json";
    std::filesystem::copy_file(
        FixturePath("CSharpScriptHostFixture.scripts.dll"),
        assemblyPath,
        std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(
        FixturePath("CSharpScriptHostFixture.scripts.runtimeconfig.json"),
        runtimeConfigPath,
        std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(
        RuntimeBridgeAssembly(),
        artifactDirectory / "SagaScript.RuntimeBridge.dll",
        std::filesystem::copy_options::overwrite_existing);

    const auto hash = "sha256:" + Sha256Hex(assemblyPath);
    const auto capabilityJson = JsonStringArray(capabilities);
    WriteFile(
        root / "Manifests" / "script_artifacts.json",
        "{\n"
        "  \"schemaVersion\": 1,\n"
        "  \"toolName\": \"sagascript\",\n"
        "  \"toolchainVersion\": \"0.0.8-dev\",\n"
        "  \"targetFramework\": \"net10.0\",\n"
        "  \"sourceHash\": \"sha256:test\",\n"
        "  \"artifacts\": [\n"
        "    {\n"
        "      \"artifactId\": \"artifact://scripts/game/player\",\n"
        "      \"scriptId\": \"script://game/player\",\n"
        "      \"assemblyPath\": \"Build/Artifacts/Scripts/CSharpScriptHostFixture.scripts.dll\",\n"
        "      \"runtimeConfigPath\": \"Build/Artifacts/Scripts/CSharpScriptHostFixture.scripts.runtimeconfig.json\",\n"
        "      \"hash\": \"" + hash + "\",\n"
        "      \"authority\": \"ServerOnly\",\n"
        "      \"packageDestinationIntent\": \"server\",\n"
        "      \"requestedCapabilities\": " + capabilityJson + ",\n"
        "      \"grantedCapabilities\": " + capabilityJson + ",\n"
        "      \"bindingIds\": [],\n"
        "      \"sourceFiles\": []\n"
        "    }\n"
        "  ],\n"
        "  \"diagnosticSummary\": {\n"
        "    \"errorCount\": 0,\n"
        "    \"warningCount\": 0,\n"
        "    \"hasBlockingDiagnostics\": false\n"
        "  }\n"
        "}\n");
    WriteFile(
        root / "Manifests" / "script_capabilities.json",
        "{\n"
        "  \"schemaVersion\": 1,\n"
        "  \"scripts\": [\n"
        "    {\n"
        "      \"scriptId\": \"script://game/player\",\n"
        "      \"authority\": \"ServerOnly\",\n"
        "      \"packageDestinationIntent\": \"server\",\n"
        "      \"requestedCapabilities\": " + capabilityJson + ",\n"
        "      \"grantedCapabilities\": " + capabilityJson + "\n"
        "    }\n"
        "  ],\n"
        "  \"diagnosticSummary\": {\n"
        "    \"errorCount\": 0,\n"
        "    \"warningCount\": 0,\n"
        "    \"hasBlockingDiagnostics\": false\n"
        "  }\n"
        "}\n");
    return root;
}

[[nodiscard]] std::vector<std::string> ReadLines(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line))
    {
        lines.push_back(line);
    }
    return lines;
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

[[nodiscard]] SagaEngine::Scripting::ScriptDiagnostic MakeTestDiagnostic(
    std::string code,
    std::string message)
{
    SagaEngine::Scripting::ScriptDiagnostic diagnostic;
    diagnostic.diagnostic.code.value = std::move(code);
    diagnostic.diagnostic.message = std::move(message);
    return diagnostic;
}

class TestScriptWorld final : public ISagaScriptWorld
{
public:
    ScriptEntityCreateResult CreateEntity(const std::string& name) override
    {
        const auto handle = nextEntityHandle_++;
        entities_.emplace(
            handle,
            EntityState{name, ScriptVector3{}});
        lastCreatedEntity_.value = handle;

        ScriptEntityCreateResult result;
        result.succeeded = true;
        result.entity.value = handle;
        return result;
    }

    ScriptHostOperationResult SetPosition(
        const ScriptEntityHandle entity,
        const ScriptVector3 position) override
    {
        ScriptHostOperationResult result;
        const auto iterator = entities_.find(entity.value);
        if (iterator == entities_.end())
        {
            result.diagnostics.push_back(MakeTestDiagnostic(
                InvalidEntityHandle,
                "Test script world received an invalid entity handle."));
            result.diagnostics.back().metadata["entityHandle"] =
                std::to_string(entity.value);
            return result;
        }

        iterator->second.position = position;
        result.succeeded = true;
        return result;
    }

    ScriptEntityPositionResult GetPosition(
        const ScriptEntityHandle entity) const override
    {
        ScriptEntityPositionResult result;
        const auto iterator = entities_.find(entity.value);
        if (iterator == entities_.end())
        {
            result.diagnostics.push_back(MakeTestDiagnostic(
                InvalidEntityHandle,
                "Test script world received an invalid entity handle."));
            result.diagnostics.back().metadata["entityHandle"] =
                std::to_string(entity.value);
            return result;
        }

        result.succeeded = true;
        result.position = iterator->second.position;
        return result;
    }

    [[nodiscard]] std::size_t EntityCount() const noexcept
    {
        return entities_.size();
    }

    [[nodiscard]] ScriptVector3 Position(
        const ScriptEntityHandle entity) const
    {
        return entities_.at(entity.value).position;
    }

    [[nodiscard]] ScriptEntityHandle LastCreatedEntity() const noexcept
    {
        return lastCreatedEntity_;
    }

private:
    struct EntityState
    {
        std::string name;
        ScriptVector3 position;
    };

    std::uint64_t nextEntityHandle_ = 1;
    ScriptEntityHandle lastCreatedEntity_;
    std::unordered_map<std::uint64_t, EntityState> entities_;
};

[[nodiscard]] ScriptInstanceCreateRequest MakeInstanceRequest(
    const SagaEngine::Scripting::ScriptPackageHandle package,
    const char* classId,
    const ScriptEntityHandle selfEntity = {})
{
    ScriptInstanceCreateRequest request;
    request.package = package;
    request.classId = ScriptClassId{classId};
    request.scriptId = "script://game/player";
    request.selfEntity = selfEntity;
    return request;
}

[[nodiscard]] ScriptUiNamedActionInvocation MakeUiActionInvocation(
    const ScriptInstanceHandle instance,
    std::string methodName)
{
    ScriptUiNamedActionInvocation invocation;
    invocation.instance = instance;
    invocation.methodName = std::move(methodName);
    invocation.context.actionId = "connect_to_server";
    invocation.context.screenId = "connect_screen";
    invocation.context.elementId = "connect_button";
    invocation.context.eventType = ScriptUiNamedActionEventType::TextChanged;
    invocation.context.text = "local-dev";
    return invocation;
}

[[nodiscard]] UiNamedActionContext MakeUiHandlerContext()
{
    UiNamedActionContext context;
    context.actionId = UiActionId::FromString("connect_to_server");
    context.screenId = UiScreenId::FromString("connect_screen");
    context.elementId = UiElementId::FromString("connect_button");
    context.eventType = UiEventType::TextChanged;
    context.text = "local-dev";
    return context;
}

} // namespace

TEST(CSharpScriptHostTests, MinimalManagedScriptLoadsAndRunsLifecycle)
{
    const auto packageRoot =
        CreateValidPackage("saga_csharp_host_valid_package");
    const auto logPath = packageRoot / "lifecycle.log";
    SetEnvironment("SAGA_CSHARP_HOST_TEST_LOG", logPath);

    CSharpScriptHost host(HostOptions());
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded());

    const auto instance = service.CreateInstance(
        loadResult.package,
        ScriptClassId{"Game.PlayerScript"});
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    EXPECT_TRUE(service.StartInstance(instance.instance).Succeeded());
    EXPECT_TRUE(service.UpdateInstance(instance.instance, 0.25).Succeeded());
    EXPECT_TRUE(service.DestroyInstance(instance.instance).Succeeded());

    const auto lines = ReadLines(logPath);
    ASSERT_EQ(lines.size(), 4u);
    EXPECT_EQ(lines[0], "OnCreate");
    EXPECT_EQ(lines[1], "OnStart");
    EXPECT_EQ(lines[2], "OnUpdate:0.25");
    EXPECT_EQ(lines[3], "OnDestroy");
}

TEST(CSharpScriptHostTests, ScriptContextIsAvailableDuringLifecycle)
{
    const auto packageRoot =
        CreateValidPackage("saga_csharp_host_context_lifecycle");
    std::vector<ScriptLogEvent> logs;

    CSharpScriptHost host(HostOptions(nullptr, &logs));
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded()) << DescribeDiagnostics(loadResult.diagnostics);
    const auto instance = service.CreateInstance(MakeInstanceRequest(
        loadResult.package,
        "Game.ContextProbeScript"));
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    EXPECT_TRUE(service.StartInstance(instance.instance).Succeeded());
    EXPECT_TRUE(service.UpdateInstance(instance.instance, 0.25).Succeeded());
    EXPECT_TRUE(service.DestroyInstance(instance.instance).Succeeded());

    ASSERT_EQ(logs.size(), 4u);
    EXPECT_EQ(logs[0].message, "Context:OnCreate:True");
    EXPECT_EQ(logs[1].message, "Context:OnStart:True");
    EXPECT_EQ(logs[2].message, "Context:OnUpdate:True:0.25");
    EXPECT_EQ(logs[3].message, "Context:OnDestroy:True");
}

TEST(CSharpScriptHostTests, DuplicateActiveEntityAndBehaviorIsRejected)
{
    const auto packageRoot = CreateValidPackage("saga_csharp_host_duplicate_identity");
    CSharpScriptHost host(HostOptions());
    ScriptLifecycleService service(host);
    const auto package = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(package.Succeeded());

    const ScriptEntityHandle entity{41};
    const auto first = service.CreateInstance(
        MakeInstanceRequest(package.package, "Game.PlayerScript", entity));
    ASSERT_TRUE(first.Succeeded());
    const auto duplicate = service.CreateInstance(
        MakeInstanceRequest(package.package, "Game.PlayerScript", entity));

    ASSERT_FALSE(duplicate.Succeeded());
    ASSERT_EQ(duplicate.diagnostics.size(), 1u);
    EXPECT_EQ(duplicate.diagnostics.front().diagnostic.code.value, InstanceCreationFailed);
    EXPECT_EQ(duplicate.diagnostics.front().metadata.at("entityHandle"), "41");
    EXPECT_EQ(duplicate.diagnostics.front().metadata.at("scriptId"), "script://game/player");
}

TEST(CSharpScriptHostTests, SuccessfulDestroyAllowsIdentityRecreation)
{
    const auto packageRoot = CreateValidPackage("saga_csharp_host_recreate_identity");
    CSharpScriptHost host(HostOptions());
    ScriptLifecycleService service(host);
    const auto package = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(package.Succeeded());
    const auto request = MakeInstanceRequest(package.package,
                                             "Game.PlayerScript",
                                             ScriptEntityHandle{42});

    const auto first = service.CreateInstance(request);
    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(service.DestroyInstance(first.instance).Succeeded());
    const auto recreated = service.CreateInstance(request);
    EXPECT_TRUE(recreated.Succeeded()) << DescribeDiagnostics(recreated.diagnostics);
}

TEST(CSharpScriptHostTests, DestroyedInstanceHandleIsStale)
{
    const auto packageRoot = CreateValidPackage("saga_csharp_host_stale_handle");
    CSharpScriptHost host(HostOptions());
    ScriptLifecycleService service(host);
    const auto package = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(package.Succeeded());
    const auto instance = service.CreateInstance(
        MakeInstanceRequest(package.package, "Game.PlayerScript", ScriptEntityHandle{43}));
    ASSERT_TRUE(instance.Succeeded());
    ASSERT_TRUE(service.DestroyInstance(instance.instance).Succeeded());

    const auto stale = service.UpdateInstance(instance.instance, 0.1);
    ASSERT_FALSE(stale.Succeeded());
    ASSERT_EQ(stale.diagnostics.size(), 1u);
    EXPECT_EQ(stale.diagnostics.front().diagnostic.code.value, InvalidInstanceHandle);
}

TEST(CSharpScriptHostTests, SameBehaviorCanRunOnTwoEntities)
{
    const auto packageRoot = CreateValidPackage("saga_csharp_host_two_entities");
    CSharpScriptHost host(HostOptions());
    ScriptLifecycleService service(host);
    const auto package = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(package.Succeeded());

    EXPECT_TRUE(service
                    .CreateInstance(MakeInstanceRequest(
                        package.package, "Game.PlayerScript", ScriptEntityHandle{51}))
                    .Succeeded());
    EXPECT_TRUE(service
                    .CreateInstance(MakeInstanceRequest(
                        package.package, "Game.PlayerScript", ScriptEntityHandle{52}))
                    .Succeeded());
}

TEST(CSharpScriptHostTests, OneEntityCanRunMultipleBehaviors)
{
    const auto packageRoot = CreateValidPackage("saga_csharp_host_two_behaviors");
    CSharpScriptHost host(HostOptions());
    ScriptLifecycleService service(host);
    const auto package = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(package.Succeeded());
    const ScriptEntityHandle entity{61};
    auto firstRequest = MakeInstanceRequest(package.package, "Game.PlayerScript", entity);
    firstRequest.scriptId = "behavior://player";
    auto secondRequest = MakeInstanceRequest(package.package, "Game.ContextProbeScript", entity);
    secondRequest.scriptId = "behavior://context-probe";

    EXPECT_TRUE(service.CreateInstance(firstRequest).Succeeded());
    EXPECT_TRUE(service.CreateInstance(secondRequest).Succeeded());
}

TEST(CSharpScriptHostTests, FailedCreatePublishesNoIdentity)
{
    const auto packageRoot = CreateValidPackage("saga_csharp_host_failed_create_identity");
    CSharpScriptHost host(HostOptions());
    ScriptLifecycleService service(host);
    const auto package = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(package.Succeeded());
    auto request = MakeInstanceRequest(package.package,
                                       "Game.MissingScript",
                                       ScriptEntityHandle{62});
    EXPECT_FALSE(service.CreateInstance(request).Succeeded());

    request.classId = ScriptClassId{"Game.PlayerScript"};
    EXPECT_TRUE(service.CreateInstance(request).Succeeded());
}

TEST(CSharpScriptHostTests, FailedMultiArtifactLoadPublishesNoPackage)
{
    const auto failedRoot = CreateValidPackage("saga_csharp_host_partial_package");
    const auto manifestPath = failedRoot / "Manifests" / "script_artifacts.json";
    std::ifstream input(manifestPath);
    auto manifest = nlohmann::json::parse(input);
    auto missing = manifest["artifacts"].front();
    missing["assemblyPath"] = "Build/Artifacts/Scripts/missing.scripts.dll";
    missing["scriptId"] = "script://game/missing";
    manifest["artifacts"].push_back(std::move(missing));
    WriteFile(manifestPath, manifest.dump(2) + "\n");

    CSharpScriptHost host(HostOptions());
    ScriptLifecycleService service(host);
    EXPECT_FALSE(service.LoadPackage(MakePackageRequest(failedRoot)).Succeeded());
    const auto validRoot = CreateValidPackage("saga_csharp_host_after_partial_package");
    const auto valid = service.LoadPackage(MakePackageRequest(validRoot));
    ASSERT_TRUE(valid.Succeeded()) << DescribeDiagnostics(valid.diagnostics);
    EXPECT_EQ(valid.package.value, 1u);
}

TEST(CSharpScriptHostTests, RepeatedHostShutdownReleasesNativeContexts)
{
    const auto packageRoot = CreateValidPackage("saga_csharp_host_repeated_shutdown");
    for (std::uint64_t entity = 70; entity < 73; ++entity)
    {
        CSharpScriptHost host(HostOptions());
        ScriptLifecycleService service(host);
        const auto package = service.LoadPackage(MakePackageRequest(packageRoot));
        ASSERT_TRUE(package.Succeeded());
        ASSERT_TRUE(service
                        .CreateInstance(MakeInstanceRequest(
                            package.package, "Game.ContextProbeScript", ScriptEntityHandle{entity}))
                        .Succeeded());
    }
}

TEST(CSharpScriptHostTests, RetainedManagedContextCannotUseDestroyedNativeContext)
{
    const auto packageRoot = CreateValidPackage("saga_csharp_host_retained_context");
    std::vector<ScriptLogEvent> logs;
    CSharpScriptHost host(HostOptions(nullptr, &logs));
    ScriptLifecycleService service(host);
    const auto package = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(package.Succeeded());

    auto retainedRequest = MakeInstanceRequest(package.package,
                                               "Game.RetainedContextScript",
                                               ScriptEntityHandle{80});
    retainedRequest.scriptId = "behavior://retained";
    const auto retained = service.CreateInstance(retainedRequest);
    ASSERT_TRUE(retained.Succeeded());
    ASSERT_TRUE(service.StartInstance(retained.instance).Succeeded());
    ASSERT_TRUE(service.DestroyInstance(retained.instance).Succeeded());

    auto probeRequest = MakeInstanceRequest(package.package,
                                            "Game.RetainedContextProbeScript",
                                            ScriptEntityHandle{80});
    probeRequest.scriptId = "behavior://probe";
    const auto probe = service.CreateInstance(probeRequest);
    ASSERT_TRUE(probe.Succeeded());
    ASSERT_TRUE(service.StartInstance(probe.instance).Succeeded());

    ASSERT_EQ(logs.size(), 1u);
    EXPECT_EQ(logs.front().message, "RetainedContext:ProbeStarted");
}

TEST(CSharpScriptHostTests, ScriptContextLogInfoIsObservedByNativeHost)
{
    const auto packageRoot =
        CreateValidPackage("saga_csharp_host_context_log");
    std::vector<ScriptLogEvent> logs;

    CSharpScriptHost host(HostOptions(nullptr, &logs));
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded()) << DescribeDiagnostics(loadResult.diagnostics);
    const auto instance = service.CreateInstance(MakeInstanceRequest(
        loadResult.package,
        "Game.ContextLogScript"));
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    EXPECT_TRUE(service.StartInstance(instance.instance).Succeeded());

    ASSERT_EQ(logs.size(), 1u);
    EXPECT_EQ(logs[0].scriptId, "script://game/player");
    EXPECT_EQ(logs[0].message, "ContextLog:OnStart");
}

TEST(CSharpScriptHostTests, ScriptContextWorldCreatesEntityAndSetsPosition)
{
    const auto packageRoot = CreateValidPackage(
        "saga_csharp_host_context_world",
        {kCreateEntityCapability, kTransformReadCapability, kTransformWriteCapability});
    TestScriptWorld world;
    std::vector<ScriptLogEvent> logs;

    CSharpScriptHost host(HostOptions(&world, &logs));
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded()) << DescribeDiagnostics(loadResult.diagnostics);
    const auto instance = service.CreateInstance(MakeInstanceRequest(
        loadResult.package,
        "Game.WorldMutationScript"));
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    EXPECT_TRUE(service.StartInstance(instance.instance).Succeeded());

    ASSERT_EQ(world.EntityCount(), 1u);
    const auto position = world.Position(world.LastCreatedEntity());
    EXPECT_FLOAT_EQ(position.x, 1.0F);
    EXPECT_FLOAT_EQ(position.y, 2.0F);
    EXPECT_FLOAT_EQ(position.z, 3.0F);
    ASSERT_FALSE(logs.empty());
    EXPECT_EQ(logs.back().message, "WorldMutation:Start:1");
}

TEST(CSharpScriptHostTests, ScriptContextOnUpdateMutatesPositionUsingDeltaTime)
{
    const auto packageRoot = CreateValidPackage(
        "saga_csharp_host_context_update_position",
        {kCreateEntityCapability, kTransformReadCapability, kTransformWriteCapability});
    TestScriptWorld world;
    std::vector<ScriptLogEvent> logs;

    CSharpScriptHost host(HostOptions(&world, &logs));
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded()) << DescribeDiagnostics(loadResult.diagnostics);
    const auto instance = service.CreateInstance(MakeInstanceRequest(
        loadResult.package,
        "Game.WorldMutationScript"));
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    ASSERT_TRUE(service.StartInstance(instance.instance).Succeeded());
    EXPECT_TRUE(service.UpdateInstance(instance.instance, 0.25).Succeeded());

    const auto position = world.Position(world.LastCreatedEntity());
    EXPECT_FLOAT_EQ(position.x, 1.25F);
    EXPECT_FLOAT_EQ(position.y, 2.0F);
    EXPECT_FLOAT_EQ(position.z, 3.0F);
    ASSERT_FALSE(logs.empty());
    EXPECT_EQ(logs.back().message, "WorldMutation:Update:0.25");
}

TEST(CSharpScriptHostTests, ScriptContextSelfEntityCanMutatePosition)
{
    const auto packageRoot = CreateValidPackage(
        "saga_csharp_host_context_self",
        {kTransformReadCapability, kTransformWriteCapability});
    TestScriptWorld world;
    const auto self = world.CreateEntity("self").entity;
    std::vector<ScriptLogEvent> logs;

    CSharpScriptHost host(HostOptions(&world, &logs));
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded()) << DescribeDiagnostics(loadResult.diagnostics);
    const auto instance = service.CreateInstance(MakeInstanceRequest(
        loadResult.package,
        "Game.SelfMutationScript",
        self));
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    ASSERT_TRUE(service.StartInstance(instance.instance).Succeeded());
    EXPECT_TRUE(service.UpdateInstance(instance.instance, 0.5).Succeeded());

    const auto position = world.Position(self);
    EXPECT_FLOAT_EQ(position.x, 4.5F);
    EXPECT_FLOAT_EQ(position.y, 5.0F);
    EXPECT_FLOAT_EQ(position.z, 6.0F);
}

TEST(CSharpScriptHostTests, InvalidScriptEntityHandleProducesDiagnostic)
{
    const auto packageRoot = CreateValidPackage(
        "saga_csharp_host_context_invalid_self",
        {kTransformWriteCapability});
    TestScriptWorld world;
    std::vector<ScriptLogEvent> logs;

    CSharpScriptHost host(HostOptions(&world, &logs));
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded()) << DescribeDiagnostics(loadResult.diagnostics);
    const auto instance = service.CreateInstance(MakeInstanceRequest(
        loadResult.package,
        "Game.InvalidSelfScript"));
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    const auto result = service.StartInstance(instance.instance);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u) << DescribeDiagnostics(result.diagnostics);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, InvalidEntityHandle);
    ASSERT_FALSE(logs.empty());
    EXPECT_EQ(logs.back().message, "InvalidSelf:SetPosition:False");
}

TEST(CSharpScriptHostTests, MissingCapabilityBlocksWorldCreateEntity)
{
    const auto packageRoot =
        CreateValidPackage("saga_csharp_host_context_missing_capability");
    TestScriptWorld world;
    std::vector<ScriptLogEvent> logs;

    CSharpScriptHost host(HostOptions(&world, &logs));
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded()) << DescribeDiagnostics(loadResult.diagnostics);
    const auto instance = service.CreateInstance(MakeInstanceRequest(
        loadResult.package,
        "Game.MissingCapabilityScript"));
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    const auto result = service.StartInstance(instance.instance);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u) << DescribeDiagnostics(result.diagnostics);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, ScriptCapabilityDenied);
    EXPECT_EQ(
        result.diagnostics[0].metadata.at("capability"),
        kCreateEntityCapability);
    EXPECT_EQ(world.EntityCount(), 0u);
    ASSERT_FALSE(logs.empty());
    EXPECT_EQ(logs.back().message, "MissingCapability:CreateEntity:False");
}

TEST(CSharpScriptHostTests, UiNamedActionInvokesManagedMethodAndPreservesContext)
{
    const auto packageRoot =
        CreateValidPackage("saga_csharp_host_ui_named_action");
    std::vector<ScriptLogEvent> logs;

    CSharpScriptHost host(HostOptions(nullptr, &logs));
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded()) << DescribeDiagnostics(loadResult.diagnostics);
    const auto instance = service.CreateInstance(MakeInstanceRequest(
        loadResult.package,
        "Game.UiNamedActionScript"));
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    CSharpUiNamedActionHandler handler(CSharpUiNamedActionHandlerOptions{
        host,
        instance.instance,
        "RecordUiAction",
    });
    const auto result = handler.Invoke(MakeUiHandlerContext());

    EXPECT_TRUE(result.Succeeded());
    EXPECT_EQ(result.code, UiNamedActionHandlerDiagnosticCode::None);
    ASSERT_EQ(logs.size(), 1u);
    EXPECT_EQ(
        logs[0].message,
        "UiAction:connect_to_server:connect_screen:connect_button:"
        "TextChanged:local-dev");
}

TEST(CSharpScriptHostTests, UiNamedActionBoolReturnTrueSucceeds)
{
    const auto packageRoot =
        CreateValidPackage("saga_csharp_host_ui_named_action_bool");
    std::vector<ScriptLogEvent> logs;

    CSharpScriptHost host(HostOptions(nullptr, &logs));
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded()) << DescribeDiagnostics(loadResult.diagnostics);
    const auto instance = service.CreateInstance(MakeInstanceRequest(
        loadResult.package,
        "Game.UiNamedActionScript"));
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    CSharpUiNamedActionHandler handler(CSharpUiNamedActionHandlerOptions{
        host,
        instance.instance,
        "BoolUiAction",
    });
    const auto result = handler.Invoke(MakeUiHandlerContext());

    EXPECT_TRUE(result.Succeeded());
    ASSERT_EQ(logs.size(), 1u);
    EXPECT_EQ(logs[0].message, "UiBool:connect_to_server:local-dev");
}

TEST(CSharpScriptHostTests, MissingUiNamedActionMethodIsDeterministic)
{
    const auto packageRoot =
        CreateValidPackage("saga_csharp_host_ui_named_action_missing");
    CSharpScriptHost host(HostOptions());
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded()) << DescribeDiagnostics(loadResult.diagnostics);
    const auto instance = service.CreateInstance(MakeInstanceRequest(
        loadResult.package,
        "Game.UiNamedActionScript"));
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    const auto result = host.InvokeUiNamedAction(
        MakeUiActionInvocation(instance.instance, "MissingUiAction"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        UiNamedActionMethodMissing);
    EXPECT_EQ(result.diagnostics[0].metadata.at("methodName"), "MissingUiAction");
}

TEST(CSharpScriptHostTests, InvalidUiNamedActionSignatureIsDeterministic)
{
    const auto packageRoot =
        CreateValidPackage("saga_csharp_host_ui_named_action_invalid_signature");
    CSharpScriptHost host(HostOptions());
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded()) << DescribeDiagnostics(loadResult.diagnostics);
    const auto instance = service.CreateInstance(MakeInstanceRequest(
        loadResult.package,
        "Game.UiNamedActionScript"));
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    const auto result = host.InvokeUiNamedAction(
        MakeUiActionInvocation(instance.instance, "InvalidUiAction"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        UiNamedActionInvalidSignature);
}

TEST(CSharpScriptHostTests, FalseUiNamedActionReturnBecomesHandlerFailure)
{
    const auto packageRoot =
        CreateValidPackage("saga_csharp_host_ui_named_action_false");
    std::vector<ScriptLogEvent> logs;

    CSharpScriptHost host(HostOptions(nullptr, &logs));
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded()) << DescribeDiagnostics(loadResult.diagnostics);
    const auto instance = service.CreateInstance(MakeInstanceRequest(
        loadResult.package,
        "Game.UiNamedActionScript"));
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    const auto scriptResult = host.InvokeUiNamedAction(
        MakeUiActionInvocation(instance.instance, "RejectUiAction"));
    ASSERT_FALSE(scriptResult.Succeeded());
    ASSERT_EQ(scriptResult.diagnostics.size(), 1u);
    EXPECT_EQ(
        scriptResult.diagnostics[0].diagnostic.code.value,
        UiNamedActionReturnedFalse);

    CSharpUiNamedActionHandler handler(CSharpUiNamedActionHandlerOptions{
        host,
        instance.instance,
        "RejectUiAction",
    });
    const auto handlerResult = handler.Invoke(MakeUiHandlerContext());

    EXPECT_FALSE(handlerResult.Succeeded());
    EXPECT_EQ(
        handlerResult.code,
        UiNamedActionHandlerDiagnosticCode::HandlerFailed);
    EXPECT_EQ(handlerResult.severity, UiDiagnosticSeverity::Error);
    ASSERT_GE(logs.size(), 2u);
    EXPECT_EQ(logs[0].message, "UiReject:connect_to_server");
}

TEST(CSharpScriptHostTests, ManagedUiNamedActionExceptionIsDeterministic)
{
    const auto packageRoot =
        CreateValidPackage("saga_csharp_host_ui_named_action_exception");
    CSharpScriptHost host(HostOptions());
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded()) << DescribeDiagnostics(loadResult.diagnostics);
    const auto instance = service.CreateInstance(MakeInstanceRequest(
        loadResult.package,
        "Game.UiNamedActionScript"));
    ASSERT_TRUE(instance.Succeeded()) << DescribeDiagnostics(instance.diagnostics);

    const auto result = host.InvokeUiNamedAction(
        MakeUiActionInvocation(instance.instance, "ThrowUiAction"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, ManagedException);
    EXPECT_EQ(result.diagnostics[0].metadata.at("methodName"), "ThrowUiAction");
}

TEST(CSharpScriptHostTests, MissingManagedClassIsDeterministic)
{
    const auto packageRoot =
        CreateValidPackage("saga_csharp_host_missing_class");
    CSharpScriptHost host(HostOptions());
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded()) << DescribeDiagnostics(loadResult.diagnostics);

    const auto instance = service.CreateInstance(
        loadResult.package,
        ScriptClassId{"Game.MissingScript"});

    ASSERT_FALSE(instance.Succeeded());
    ASSERT_EQ(instance.diagnostics.size(), 1u);
    EXPECT_EQ(instance.diagnostics[0].diagnostic.code.value, ClassMissing);
}

TEST(CSharpScriptHostTests, ClassNotDerivingFromSagaScriptIsInvalid)
{
    const auto packageRoot =
        CreateValidPackage("saga_csharp_host_invalid_class");
    CSharpScriptHost host(HostOptions());
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded()) << DescribeDiagnostics(loadResult.diagnostics);

    const auto instance = service.CreateInstance(
        loadResult.package,
        ScriptClassId{"Game.NotASagaScript"});

    ASSERT_FALSE(instance.Succeeded());
    ASSERT_EQ(instance.diagnostics.size(), 1u);
    EXPECT_EQ(instance.diagnostics[0].diagnostic.code.value, ClassInvalid);
}

TEST(CSharpScriptHostTests, ManagedLifecycleExceptionIsDeterministic)
{
    const auto packageRoot =
        CreateValidPackage("saga_csharp_host_lifecycle_exception");
    CSharpScriptHost host(HostOptions());
    ScriptLifecycleService service(host);

    const auto loadResult = service.LoadPackage(MakePackageRequest(packageRoot));
    ASSERT_TRUE(loadResult.Succeeded()) << DescribeDiagnostics(loadResult.diagnostics);
    const auto instance = service.CreateInstance(
        loadResult.package,
        ScriptClassId{"Game.ThrowingScript"});
    ASSERT_TRUE(instance.Succeeded());

    const auto result = service.StartInstance(instance.instance);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, ManagedException);
    EXPECT_EQ(result.diagnostics[0].metadata.at("lifecycleMethod"), "OnStart");
}

TEST(CSharpScriptHostTests, HostReportsMissingRuntimeConfigDefensively)
{
    const auto packageRoot =
        CreateValidPackage("saga_csharp_host_missing_runtimeconfig");
    std::filesystem::remove(packageRoot / "Build" / "Artifacts" / "Scripts" /
        "CSharpScriptHostFixture.scripts.runtimeconfig.json");

    CSharpScriptHost host(HostOptions());
    const auto result = host.LoadPackage(MakePackageRequest(packageRoot));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, RuntimeConfigMissing);
}

TEST(CSharpScriptHostTests, HostReportsInvalidRuntimeConfigDefensively)
{
    const auto packageRoot =
        CreateValidPackage("saga_csharp_host_invalid_runtimeconfig");
    WriteFile(
        packageRoot / "Build" / "Artifacts" / "Scripts" /
            "CSharpScriptHostFixture.scripts.runtimeconfig.json",
        "{ invalid json");

    CSharpScriptHost host(HostOptions());
    const auto result = host.LoadPackage(MakePackageRequest(packageRoot));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, RuntimeConfigInvalid);
}

TEST(CSharpScriptHostTests, HostReportsMissingAssemblyDefensively)
{
    const auto packageRoot =
        CreateValidPackage("saga_csharp_host_missing_assembly");
    std::filesystem::remove(packageRoot / "Build" / "Artifacts" / "Scripts" /
        "CSharpScriptHostFixture.scripts.dll");

    CSharpScriptHost host(HostOptions());
    const auto result = host.LoadPackage(MakePackageRequest(packageRoot));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, AssemblyLoadFailed);
}

TEST(CSharpScriptHostTests, VerifiedGenerationLoadsExactManifestSet)
{
    auto fixture = CreateVerifiedGeneration("saga_verified_generation_valid");
    CSharpScriptHost host(HostOptions());

    const auto result = host.LoadVerifiedGeneration(fixture.request);

    ASSERT_TRUE(result.Succeeded()) << DescribeDiagnostics(result.diagnostics);
}

TEST(CSharpScriptHostTests, VerifiedGenerationRejectsTamperedAssemblyManifest)
{
    auto fixture = CreateVerifiedGeneration("saga_verified_generation_tampered");
    WriteFile(fixture.request.assemblyManifest.path, "{}\n");
    CSharpScriptHost host(HostOptions());

    const auto result = host.LoadVerifiedGeneration(fixture.request);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.front().diagnostic.code.value,
        SagaEngine::Scripting::ScriptHostDiagnostics::ArtifactHashMismatch);
}

TEST(CSharpScriptHostTests, VerifiedGenerationRejectsManifestIdentityMismatch)
{
    auto fixture = CreateVerifiedGeneration("saga_verified_generation_identity");
    auto manifest = ReadJson(fixture.request.behaviorManifest);
    manifest["sourceSetHash"] = "sha256:another-source-set";
    RewriteVerifiedJson(fixture.request.behaviorManifest, manifest);
    CSharpScriptHost host(HostOptions());

    const auto result = host.LoadVerifiedGeneration(fixture.request);

    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(result.diagnostics.front().diagnostic.code.value,
        SagaEngine::Scripting::ScriptHostDiagnostics::BehaviorManifestMismatch);
}

TEST(CSharpScriptHostTests, VerifiedGenerationRejectsUngrantableBehaviorCapability)
{
    auto fixture = CreateVerifiedGeneration("saga_verified_generation_capability");
    auto manifest = ReadJson(fixture.request.behaviorManifest);
    manifest["behaviors"][0]["capabilities"].push_back("Gameplay.Unverified");
    RewriteVerifiedJson(fixture.request.behaviorManifest, manifest);
    CSharpScriptHost host(HostOptions());

    const auto result = host.LoadVerifiedGeneration(fixture.request);

    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(result.diagnostics.front().diagnostic.code.value,
        SagaEngine::Scripting::ScriptHostDiagnostics::CapabilityGrantUnexpected);
}

TEST(CSharpScriptHostTests, VerifiedGenerationRejectsDuplicateBehaviorAndActionIdentities)
{
    auto behaviorFixture = CreateVerifiedGeneration("saga_verified_generation_duplicate_behavior");
    auto behaviors = ReadJson(behaviorFixture.request.behaviorManifest);
    behaviors["behaviors"].push_back(behaviors["behaviors"][0]);
    RewriteVerifiedJson(behaviorFixture.request.behaviorManifest, behaviors);
    CSharpScriptHost behaviorHost(HostOptions());
    const auto behaviorResult = behaviorHost.LoadVerifiedGeneration(behaviorFixture.request);
    ASSERT_FALSE(behaviorResult.Succeeded());
    EXPECT_EQ(behaviorResult.diagnostics.front().diagnostic.code.value,
        SagaEngine::Scripting::ScriptHostDiagnostics::ManifestInvalid);

    auto actionFixture = CreateVerifiedGeneration("saga_verified_generation_duplicate_action");
    auto actions = ReadJson(actionFixture.request.namedActionManifest);
    actions["actions"].push_back(actions["actions"][0]);
    RewriteVerifiedJson(actionFixture.request.namedActionManifest, actions);
    CSharpScriptHost actionHost(HostOptions());
    const auto actionResult = actionHost.LoadVerifiedGeneration(actionFixture.request);
    ASSERT_FALSE(actionResult.Succeeded());
    EXPECT_EQ(actionResult.diagnostics.front().diagnostic.code.value,
        SagaEngine::Scripting::ScriptHostDiagnostics::ManifestInvalid);
}

TEST(CSharpScriptHostTests, VerifiedGenerationRejectsRuntimeBridgeMismatch)
{
    auto fixture = CreateVerifiedGeneration("saga_verified_generation_bridge");
    fixture.request.runtimeBridgeHash = "sha256:wrong-runtime-bridge";
    auto assembly = ReadJson(fixture.request.assemblyManifest);
    assembly["runtimeBridgeHash"] = fixture.request.runtimeBridgeHash;
    RewriteVerifiedJson(fixture.request.assemblyManifest, assembly);
    CSharpScriptHost host(HostOptions());

    const auto result = host.LoadVerifiedGeneration(fixture.request);

    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(result.diagnostics.front().diagnostic.code.value,
        SagaEngine::Scripting::ScriptHostDiagnostics::ArtifactHashMismatch);
}

TEST(CSharpScriptHostTests, VerifiedGenerationRejectsMalformedManifestFieldTypes)
{
    auto fixture = CreateVerifiedGeneration("saga_verified_generation_field_types");
    auto behaviors = ReadJson(fixture.request.behaviorManifest);
    behaviors["behaviors"][0] = 7;
    RewriteVerifiedJson(fixture.request.behaviorManifest, behaviors);
    CSharpScriptHost host(HostOptions());

    const auto result = host.LoadVerifiedGeneration(fixture.request);

    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(result.diagnostics.front().diagnostic.code.value,
        SagaEngine::Scripting::ScriptHostDiagnostics::ManifestInvalid);
}

TEST(CSharpScriptHostTests, VerifiedGenerationRejectsMissingNamedActionAttributeMapping)
{
    auto fixture = CreateVerifiedGeneration("saga_verified_generation_action_mapping");
    auto actions = ReadJson(fixture.request.namedActionManifest);
    actions["actions"][0].erase("attributeTypeId");
    RewriteVerifiedJson(fixture.request.namedActionManifest, actions);
    CSharpScriptHost host(HostOptions());

    const auto result = host.LoadVerifiedGeneration(fixture.request);

    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(result.diagnostics.front().diagnostic.code.value,
        SagaEngine::Scripting::ScriptHostDiagnostics::ManifestInvalid);
}

TEST(CSharpScriptHostTests, VerifiedGenerationRejectsWrongReferenceIdentity)
{
    auto fixture = CreateVerifiedGeneration("saga_verified_generation_reference_identity");
    fixture.request.referenceManifestHash = "sha256:other-references";
    CSharpScriptHost host(HostOptions());

    const auto result = host.LoadVerifiedGeneration(fixture.request);

    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(result.diagnostics.front().diagnostic.code.value,
        SagaEngine::Scripting::ScriptHostDiagnostics::ReferenceIdentityMismatch);
}
