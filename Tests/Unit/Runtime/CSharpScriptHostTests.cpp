/// @file CSharpScriptHostTests.cpp
/// @brief Focused tests for the hostfxr-backed C# SagaScript host.

#include <SagaEngine/Scripting/CSharpScriptHost.hpp>
#include <SagaEngine/Scripting/ScriptLifecycleService.hpp>

#include <gtest/gtest.h>
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
using SagaEngine::Scripting::ScriptHostDiagnostics::ManagedException;
using SagaEngine::Scripting::ScriptHostDiagnostics::RuntimeConfigInvalid;
using SagaEngine::Scripting::ScriptHostDiagnostics::RuntimeConfigMissing;
using SagaEngine::Scripting::ScriptHostDiagnostics::ScriptCapabilityDenied;
using SagaEngine::Scripting::ScriptInstanceCreateRequest;
using SagaEngine::Scripting::ScriptLifecycleService;
using SagaEngine::Scripting::ScriptLogEvent;
using SagaEngine::Scripting::ScriptPackageLoadRequest;
using SagaEngine::Scripting::ScriptVector3;

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
