/// @file ScriptLifecycleServiceTests.cpp
/// @brief Unit tests for editor-independent SagaScript lifecycle hosting.

#include <SagaEngine/Scripting/ScriptLifecycleService.hpp>

#include "SagaShared/Diagnostics/DiagnosticCategory.hpp"
#include "SagaShared/Diagnostics/DiagnosticSeverity.hpp"
#include "SagaShared/Diagnostics/DiagnosticSource.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

using SagaEngine::Scripting::ISagaScriptHost;
using SagaEngine::Scripting::ScriptClassId;
using SagaEngine::Scripting::ScriptDiagnostic;
using SagaEngine::Scripting::ScriptHostDiagnostics::ArtifactHashMismatch;
using SagaEngine::Scripting::ScriptHostDiagnostics::ClassMissing;
using SagaEngine::Scripting::ScriptHostDiagnostics::ArtifactMissing;
using SagaEngine::Scripting::ScriptHostDiagnostics::CapabilityGrantMissing;
using SagaEngine::Scripting::ScriptHostDiagnostics::CapabilityGrantUnexpected;
using SagaEngine::Scripting::ScriptHostDiagnostics::CapabilityManifestInvalid;
using SagaEngine::Scripting::ScriptHostDiagnostics::CapabilityManifestMismatch;
using SagaEngine::Scripting::ScriptHostDiagnostics::CapabilityManifestMissing;
using SagaEngine::Scripting::ScriptHostDiagnostics::InvalidArtifactHash;
using SagaEngine::Scripting::ScriptHostDiagnostics::InvalidArtifactPath;
using SagaEngine::Scripting::ScriptHostDiagnostics::InvalidAuthority;
using SagaEngine::Scripting::ScriptHostDiagnostics::InvalidCapabilityMetadata;
using SagaEngine::Scripting::ScriptHostDiagnostics::InvalidInstanceHandle;
using SagaEngine::Scripting::ScriptHostDiagnostics::LifecycleFailed;
using SagaEngine::Scripting::ScriptHostDiagnostics::ManifestMissing;
using SagaEngine::Scripting::ScriptHostDiagnostics::PackageDestinationMismatch;
using SagaEngine::Scripting::ScriptHostDiagnostics::UnsupportedTargetFramework;
using SagaEngine::Scripting::ScriptHostOperationResult;
using SagaEngine::Scripting::ScriptInstanceCreateResult;
using SagaEngine::Scripting::ScriptInstanceHandle;
using SagaEngine::Scripting::ScriptLifecycleInvocation;
using SagaEngine::Scripting::ScriptLifecycleMethod;
using SagaEngine::Scripting::ScriptLifecycleService;
using SagaEngine::Scripting::ScriptPackageValidationOptions;
using SagaEngine::Scripting::ScriptPackageHandle;
using SagaEngine::Scripting::ScriptPackageLoadRequest;
using SagaEngine::Scripting::ScriptPackageLoadResult;

/// Recorded lifecycle invocation for deterministic mock assertions.
struct RecordedLifecycleCall
{
    ScriptLifecycleMethod method = ScriptLifecycleMethod::OnCreate; ///< Called lifecycle method.
    ScriptInstanceHandle instance; ///< Target instance passed to the host.
    double deltaTimeSeconds = 0.0; ///< Delta time received by the host.
};

constexpr const char* kDllArtifactSha256 =
    "0343b13708cd52d33ecbb1043b5ae145e73a532bdccf7de6925db5cce6054fd3";
constexpr const char* kDllArtifactHash =
    "sha256:0343b13708cd52d33ecbb1043b5ae145e73a532bdccf7de6925db5cce6054fd3";

[[nodiscard]] ScriptDiagnostic MakeDiagnostic(
    std::string code,
    std::string title,
    std::string message)
{
    ScriptDiagnostic diagnostic;
    diagnostic.diagnostic.severity =
        SagaShared::Diagnostics::DiagnosticSeverity::Error;
    diagnostic.diagnostic.category =
        SagaShared::Diagnostics::DiagnosticCategory::Script;
    diagnostic.diagnostic.source =
        SagaShared::Diagnostics::DiagnosticSource::Runtime;
    diagnostic.diagnostic.code.value = std::move(code);
    diagnostic.diagnostic.title = std::move(title);
    diagnostic.diagnostic.message = std::move(message);
    return diagnostic;
}

/// Deterministic test host that simulates script package and lifecycle behavior.
class MockScriptHost final : public ISagaScriptHost
{
public:
    std::unordered_set<std::string> availableClasses; ///< Classes that can be instantiated.
    std::optional<ScriptLifecycleMethod> failingMethod; ///< Lifecycle method that should fail.
    std::vector<RecordedLifecycleCall> lifecycleCalls; ///< Ordered lifecycle calls received.
    std::size_t loadPackageCallCount = 0; ///< Number of package loads received.

    [[nodiscard]] ScriptPackageLoadResult LoadPackage(
        const ScriptPackageLoadRequest& request) override
    {
        ++loadPackageCallCount;
        lastLoadRequest = request;

        ScriptPackageLoadResult result;
        result.succeeded = true;
        result.package.value = nextPackageHandle_++;
        return result;
    }

    [[nodiscard]] ScriptInstanceCreateResult CreateInstance(
        const ScriptPackageHandle package,
        const ScriptClassId& classId) override
    {
        lastPackage = package;
        lastClassId = classId;

        ScriptInstanceCreateResult result;
        if (availableClasses.find(classId.value) == availableClasses.end())
        {
            result.succeeded = false;
            result.diagnostics.push_back(MakeDiagnostic(
                ClassMissing,
                "Script class not found",
                "The requested script class is not available in the loaded package."));
            result.diagnostics.front().scriptId = classId.value;
            return result;
        }

        result.succeeded = true;
        result.instance.value = nextInstanceHandle_++;
        return result;
    }

    [[nodiscard]] ScriptHostOperationResult InvokeLifecycle(
        const ScriptLifecycleInvocation& invocation) override
    {
        lifecycleCalls.push_back(RecordedLifecycleCall{
            invocation.method,
            invocation.instance,
            invocation.deltaTimeSeconds,
        });

        ScriptHostOperationResult result;
        if (failingMethod.has_value() && *failingMethod == invocation.method)
        {
            result.succeeded = false;
            result.diagnostics.push_back(MakeDiagnostic(
                LifecycleFailed,
                "Script lifecycle failed",
                "The mock script host failed the requested lifecycle method."));
            result.diagnostics.front().metadata["lifecycleMethod"] =
                std::string(SagaEngine::Scripting::ToString(invocation.method));
            return result;
        }

        result.succeeded = true;
        return result;
    }

    ScriptPackageLoadRequest lastLoadRequest; ///< Last package load request received.
    ScriptPackageHandle lastPackage;          ///< Last package handle passed to CreateInstance.
    ScriptClassId lastClassId;                ///< Last class id passed to CreateInstance.

private:
    std::uint64_t nextPackageHandle_ = 100;  ///< Deterministic package handle source.
    std::uint64_t nextInstanceHandle_ = 200; ///< Deterministic instance handle source.
};

void WriteFile(const std::filesystem::path& path, const std::string& contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << contents;
}

[[nodiscard]] std::filesystem::path TempPackageRoot(const char* name)
{
    const auto root = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

[[nodiscard]] std::string ScriptArtifactManifestJson(
    const char* assemblyPath = "Build/Artifacts/Scripts/Game.scripts.dll",
    const char* runtimeConfigPath =
        "Build/Artifacts/Scripts/Game.scripts.runtimeconfig.json",
    const char* destination = "server",
    const char* authority = "ServerOnly",
    const char* hash = kDllArtifactHash,
    const char* targetFramework = "net10.0",
    const char* requestedCapabilities = "[]",
    const char* grantedCapabilities = "[]")
{
    std::ostringstream manifest;
    manifest
        << "{\n"
        << "  \"schemaVersion\": 1,\n"
        << "  \"toolName\": \"sagascript\",\n"
        << "  \"toolchainVersion\": \"0.0.8-dev\",\n"
        << "  \"targetFramework\": \"" << targetFramework << "\",\n"
        << "  \"sourceHash\": \"sha256:test\",\n"
        << "  \"artifacts\": [\n"
        << "    {\n"
        << "      \"artifactId\": \"artifact://scripts/game/player\",\n"
        << "      \"scriptId\": \"script://game/player\",\n"
        << "      \"assemblyPath\": \"" << assemblyPath << "\",\n"
        << "      \"runtimeConfigPath\": \"" << runtimeConfigPath << "\",\n"
        << "      \"hash\": \"" << hash << "\",\n"
        << "      \"authority\": \"" << authority << "\",\n"
        << "      \"packageDestinationIntent\": \"" << destination << "\",\n"
        << "      \"requestedCapabilities\": " << requestedCapabilities << ",\n"
        << "      \"grantedCapabilities\": " << grantedCapabilities << ",\n"
        << "      \"bindingIds\": [],\n"
        << "      \"sourceFiles\": []\n"
        << "    }\n"
        << "  ],\n"
        << "  \"diagnosticSummary\": {\n"
        << "    \"errorCount\": 0,\n"
        << "    \"warningCount\": 0,\n"
        << "    \"hasBlockingDiagnostics\": false\n"
        << "  }\n"
        << "}\n";
    return manifest.str();
}

[[nodiscard]] std::string ScriptCapabilityManifestJson(
    const char* scriptId = "script://game/player",
    const char* destination = "server",
    const char* authority = "ServerOnly",
    const char* requestedCapabilities = "[]",
    const char* grantedCapabilities = "[]")
{
    std::ostringstream manifest;
    manifest
        << "{\n"
        << "  \"schemaVersion\": 1,\n"
        << "  \"toolName\": \"sagascript\",\n"
        << "  \"toolchainVersion\": \"0.0.8-dev\",\n"
        << "  \"sourceHash\": \"sha256:test\",\n"
        << "  \"scripts\": [\n"
        << "    {\n"
        << "      \"scriptId\": \"" << scriptId << "\",\n"
        << "      \"authority\": \"" << authority << "\",\n"
        << "      \"packageDestinationIntent\": \"" << destination << "\",\n"
        << "      \"requestedCapabilities\": " << requestedCapabilities << ",\n"
        << "      \"grantedCapabilities\": " << grantedCapabilities << "\n"
        << "    }\n"
        << "  ],\n"
        << "  \"diagnosticSummary\": {\n"
        << "    \"errorCount\": 0,\n"
        << "    \"warningCount\": 0,\n"
        << "    \"hasBlockingDiagnostics\": false\n"
        << "  }\n"
        << "}\n";
    return manifest.str();
}

[[nodiscard]] ScriptPackageLoadRequest MakePackageRequest(
    const std::filesystem::path& packageRoot)
{
    ScriptPackageLoadRequest request;
    request.packageRoot = packageRoot;
    request.scriptArtifactManifest = "Manifests/script_artifacts.json";
    request.packageId = "test.package";
    return request;
}

[[nodiscard]] ScriptPackageLoadRequest MakeValidPackageRequest(
    const char* name,
    const char* destination = "server",
    const char* authority = "ServerOnly",
    const char* hash = kDllArtifactHash,
    const char* targetFramework = "net10.0",
    const char* requestedCapabilities = "[]",
    const char* grantedCapabilities = "[]")
{
    const auto root = TempPackageRoot(name);
    const char* capabilityAuthority =
        authority[0] == '\0' ? "ServerOnly" : authority;
    const char* capabilityRequested =
        requestedCapabilities[0] == '[' ? requestedCapabilities : "[]";
    const char* capabilityGranted =
        grantedCapabilities[0] == '[' ? grantedCapabilities : "[]";
    WriteFile(
        root / "Manifests" / "script_artifacts.json",
        ScriptArtifactManifestJson(
            "Build/Artifacts/Scripts/Game.scripts.dll",
            "Build/Artifacts/Scripts/Game.scripts.runtimeconfig.json",
            destination,
            authority,
            hash,
            targetFramework,
            requestedCapabilities,
            grantedCapabilities));
    WriteFile(
        root / "Manifests" / "script_capabilities.json",
        ScriptCapabilityManifestJson(
            "script://game/player",
            destination,
            capabilityAuthority,
            capabilityRequested,
            capabilityGranted));
    WriteFile(root / "Build" / "Artifacts" / "Scripts" / "Game.scripts.dll", "dll");
    WriteFile(
        root / "Build" / "Artifacts" / "Scripts" /
            "Game.scripts.runtimeconfig.json",
        R"({"runtimeOptions":{"tfm":"net10.0"}})");
    return MakePackageRequest(root);
}

[[nodiscard]] std::string Sha256Hash(const std::string& hex)
{
    return "sha256:" + hex;
}

[[nodiscard]] std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

[[nodiscard]] bool ContainsForbiddenToken(const std::string& text)
{
    const std::vector<std::string> forbidden = {
        "SagaEditor",
        "Editor/",
        "Qt",
        "CoreCLR",
        "hostfxr",
        "nethost",
        "Roslyn",
        "Microsoft.CodeAnalysis",
    };

    return std::any_of(
        forbidden.begin(),
        forbidden.end(),
        [&text](const std::string& token) {
            return text.find(token) != std::string::npos;
        });
}

} // namespace

TEST(ScriptLifecycleServiceTests, LifecycleOrderIsDeterministic)
{
    MockScriptHost host;
    host.availableClasses.insert("Game.PlayerController");
    ScriptLifecycleService service(host);

    const auto package = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_order"));
    ASSERT_TRUE(package.Succeeded());

    const auto instance =
        service.CreateInstance(package.package, ScriptClassId{"Game.PlayerController"});
    ASSERT_TRUE(instance.Succeeded());

    EXPECT_TRUE(service.StartInstance(instance.instance).Succeeded());
    EXPECT_TRUE(service.UpdateInstance(instance.instance, 0.016).Succeeded());
    EXPECT_TRUE(service.DestroyInstance(instance.instance).Succeeded());

    ASSERT_EQ(host.lifecycleCalls.size(), 4u);
    EXPECT_EQ(host.lifecycleCalls[0].method, ScriptLifecycleMethod::OnCreate);
    EXPECT_EQ(host.lifecycleCalls[1].method, ScriptLifecycleMethod::OnStart);
    EXPECT_EQ(host.lifecycleCalls[2].method, ScriptLifecycleMethod::OnUpdate);
    EXPECT_EQ(host.lifecycleCalls[3].method, ScriptLifecycleMethod::OnDestroy);
}

TEST(ScriptLifecycleServiceTests, OnUpdateReceivesDeltaTime)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    ASSERT_TRUE(service.UpdateInstance(ScriptInstanceHandle{42}, 0.033).Succeeded());

    ASSERT_EQ(host.lifecycleCalls.size(), 1u);
    EXPECT_EQ(host.lifecycleCalls[0].method, ScriptLifecycleMethod::OnUpdate);
    EXPECT_DOUBLE_EQ(host.lifecycleCalls[0].deltaTimeSeconds, 0.033);
}

TEST(ScriptLifecycleServiceTests, MissingClassProducesDeterministicDiagnostic)
{
    MockScriptHost host;
    std::vector<ScriptDiagnostic> diagnostics;
    ScriptLifecycleService service(
        host,
        [&diagnostics](const ScriptDiagnostic& diagnostic) {
            diagnostics.push_back(diagnostic);
        });

    const auto package = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_missing_class"));
    ASSERT_TRUE(package.Succeeded());

    const auto instance =
        service.CreateInstance(package.package, ScriptClassId{"Game.Missing"});

    ASSERT_FALSE(instance.Succeeded());
    ASSERT_EQ(instance.diagnostics.size(), 1u);
    EXPECT_EQ(instance.diagnostics[0].diagnostic.code.value, ClassMissing);
    ASSERT_EQ(diagnostics.size(), 1u);
    EXPECT_EQ(diagnostics[0].diagnostic.code.value, ClassMissing);
}

TEST(ScriptLifecycleServiceTests, ValidPackageLoadReachesHost)
{
    MockScriptHost host;
    ScriptPackageValidationOptions options;
    options.expectedPackageDestination = "server";
    ScriptLifecycleService service(host, {}, options);

    const auto request =
        MakeValidPackageRequest("saga_script_lifecycle_valid_package");
    const auto result = service.LoadPackage(request);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_TRUE(result.package.IsValid());
    EXPECT_EQ(host.loadPackageCallCount, 1u);
    EXPECT_EQ(host.lastLoadRequest.packageRoot, request.packageRoot);
}

TEST(ScriptLifecycleServiceTests, EmptyCapabilityGrantsReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto result = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_empty_capability_grants",
        "server",
        "ServerOnly",
        kDllArtifactHash,
        "net10.0",
        "[]",
        "[]"));

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(host.loadPackageCallCount, 1u);
}

TEST(ScriptLifecycleServiceTests, ExactCapabilityGrantsReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto result = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_exact_capability_grants",
        "server",
        "ServerOnly",
        kDllArtifactHash,
        "net10.0",
        "[\"Gameplay.Inventory.Write\"]",
        "[\"Gameplay.Inventory.Write\"]"));

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(host.loadPackageCallCount, 1u);
}

TEST(ScriptLifecycleServiceTests, MissingCapabilityGrantDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto result = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_missing_capability_grant",
        "server",
        "ServerOnly",
        kDllArtifactHash,
        "net10.0",
        "[\"Gameplay.Inventory.Write\"]",
        "[]"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        CapabilityGrantMissing);
    EXPECT_EQ(
        result.diagnostics[0].metadata.at("capability"),
        "Gameplay.Inventory.Write");
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, UnexpectedCapabilityGrantDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto result = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_unexpected_capability_grant",
        "server",
        "ServerOnly",
        kDllArtifactHash,
        "net10.0",
        "[]",
        "[\"Gameplay.Inventory.Write\"]"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        CapabilityGrantUnexpected);
    EXPECT_EQ(
        result.diagnostics[0].metadata.at("capability"),
        "Gameplay.Inventory.Write");
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, MultipleCapabilityGrantFailuresAreDeterministic)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto result = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_multiple_capability_grant_failures",
        "server",
        "ServerOnly",
        kDllArtifactHash,
        "net10.0",
        "[\"Gameplay.Quest.Read\",\"Gameplay.Inventory.Write\"]",
        "[\"Gameplay.World.Time.Read\",\"Gameplay.Inventory.Read\"]"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 4u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        CapabilityGrantMissing);
    EXPECT_EQ(
        result.diagnostics[0].metadata.at("capability"),
        "Gameplay.Inventory.Write");
    EXPECT_EQ(
        result.diagnostics[1].diagnostic.code.value,
        CapabilityGrantMissing);
    EXPECT_EQ(
        result.diagnostics[1].metadata.at("capability"),
        "Gameplay.Quest.Read");
    EXPECT_EQ(
        result.diagnostics[2].diagnostic.code.value,
        CapabilityGrantUnexpected);
    EXPECT_EQ(
        result.diagnostics[2].metadata.at("capability"),
        "Gameplay.Inventory.Read");
    EXPECT_EQ(
        result.diagnostics[3].diagnostic.code.value,
        CapabilityGrantUnexpected);
    EXPECT_EQ(
        result.diagnostics[3].metadata.at("capability"),
        "Gameplay.World.Time.Read");
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, MissingManifestDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);
    const auto request = MakePackageRequest(
        TempPackageRoot("saga_script_lifecycle_missing_manifest"));

    const auto result = service.LoadPackage(request);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, ManifestMissing);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, MissingScriptAssemblyDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);
    const auto root = TempPackageRoot("saga_script_lifecycle_missing_assembly");
    WriteFile(
        root / "Manifests" / "script_artifacts.json",
        ScriptArtifactManifestJson());
    WriteFile(
        root / "Manifests" / "script_capabilities.json",
        ScriptCapabilityManifestJson());
    WriteFile(
        root / "Build" / "Artifacts" / "Scripts" /
            "Game.scripts.runtimeconfig.json",
        R"({"runtimeOptions":{"tfm":"net10.0"}})");

    const auto result = service.LoadPackage(MakePackageRequest(root));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, ArtifactMissing);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, EscapingArtifactPathDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);
    const auto root = TempPackageRoot("saga_script_lifecycle_escaping_path");
    WriteFile(
        root / "Manifests" / "script_artifacts.json",
        ScriptArtifactManifestJson(
            "../outside/Game.scripts.dll",
            "Build/Artifacts/Scripts/Game.scripts.runtimeconfig.json"));
    WriteFile(
        root / "Manifests" / "script_capabilities.json",
        ScriptCapabilityManifestJson());
    WriteFile(
        root / "Build" / "Artifacts" / "Scripts" /
            "Game.scripts.runtimeconfig.json",
        R"({"runtimeOptions":{"tfm":"net10.0"}})");

    const auto result = service.LoadPackage(MakePackageRequest(root));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, InvalidArtifactPath);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, PackageDestinationMismatchDoesNotReachHost)
{
    MockScriptHost host;
    ScriptPackageValidationOptions options;
    options.expectedPackageDestination = "server";
    ScriptLifecycleService service(host, {}, options);

    const auto result = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_destination_mismatch",
        "client"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        PackageDestinationMismatch);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, UnsupportedTargetFrameworkDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto result = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_unsupported_tfm",
        "server",
        "ServerOnly",
        kDllArtifactHash,
        "net8.0"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        UnsupportedTargetFramework);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, MissingArtifactHashDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto result = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_missing_hash",
        "server",
        "ServerOnly",
        ""));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, InvalidArtifactHash);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, ArtifactHashWithoutSha256PrefixDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto result = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_bad_hash_prefix",
        "server",
        "ServerOnly",
        "abc123"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, InvalidArtifactHash);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, MalformedSha256ArtifactHashDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto result = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_malformed_sha256_hash",
        "server",
        "ServerOnly",
        "sha256:not-hex"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, InvalidArtifactHash);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, ChangedAssemblyHashDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto request =
        MakeValidPackageRequest("saga_script_lifecycle_hash_mismatch");
    WriteFile(
        request.packageRoot / "Build" / "Artifacts" / "Scripts" /
            "Game.scripts.dll",
        "changed-dll");

    const auto result = service.LoadPackage(request);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        ArtifactHashMismatch);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, UppercaseArtifactHashIsAccepted)
{
    std::string uppercaseHash = kDllArtifactSha256;
    std::transform(
        uppercaseHash.begin(),
        uppercaseHash.end(),
        uppercaseHash.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });

    MockScriptHost host;
    ScriptLifecycleService service(host);
    const auto manifestHash = Sha256Hash(uppercaseHash);

    const auto result = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_uppercase_hash",
        "server",
        "ServerOnly",
        manifestHash.c_str()));

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(host.loadPackageCallCount, 1u);
}

TEST(ScriptLifecycleServiceTests, MissingAuthorityDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto result = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_missing_authority",
        "server",
        ""));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, InvalidAuthority);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, ClientAuthorityInServerPackageDoesNotReachHost)
{
    MockScriptHost host;
    ScriptPackageValidationOptions options;
    options.expectedPackageDestination = "server";
    ScriptLifecycleService service(host, {}, options);

    const auto result = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_client_authority_in_server",
        "server",
        "ClientOnly"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, InvalidAuthority);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, ServerAuthorityInClientPackageDoesNotReachHost)
{
    MockScriptHost host;
    ScriptPackageValidationOptions options;
    options.expectedPackageDestination = "client";
    ScriptLifecycleService service(host, {}, options);

    const auto result = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_server_authority_in_client",
        "client",
        "ServerOnly"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, InvalidAuthority);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, MalformedCapabilityMetadataDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto result = service.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_malformed_capabilities",
        "server",
        "ServerOnly",
        kDllArtifactHash,
        "net10.0",
        "\"Gameplay.Inventory.Write\""));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        InvalidCapabilityMetadata);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, MalformedCapabilityArrayDoesNotEmitGrantDiagnostics)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto request = MakeValidPackageRequest(
        "saga_script_lifecycle_malformed_capability_array");
    WriteFile(
        request.packageRoot / "Manifests" / "script_artifacts.json",
        ScriptArtifactManifestJson(
            "Build/Artifacts/Scripts/Game.scripts.dll",
            "Build/Artifacts/Scripts/Game.scripts.runtimeconfig.json",
            "server",
            "ServerOnly",
            kDllArtifactHash,
            "net10.0",
            "[123]",
            "[]"));

    const auto result = service.LoadPackage(request);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        InvalidCapabilityMetadata);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, MissingCapabilityManifestDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);
    const auto root = TempPackageRoot(
        "saga_script_lifecycle_missing_capability_manifest");
    WriteFile(
        root / "Manifests" / "script_artifacts.json",
        ScriptArtifactManifestJson());
    WriteFile(root / "Build" / "Artifacts" / "Scripts" / "Game.scripts.dll", "dll");
    WriteFile(
        root / "Build" / "Artifacts" / "Scripts" /
            "Game.scripts.runtimeconfig.json",
        R"({"runtimeOptions":{"tfm":"net10.0"}})");

    const auto result = service.LoadPackage(MakePackageRequest(root));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        CapabilityManifestMissing);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, MalformedCapabilityManifestDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);
    const auto root = TempPackageRoot(
        "saga_script_lifecycle_malformed_capability_manifest");
    WriteFile(
        root / "Manifests" / "script_artifacts.json",
        ScriptArtifactManifestJson());
    WriteFile(
        root / "Manifests" / "script_capabilities.json",
        R"({"schemaVersion":1,"scripts":{}})");
    WriteFile(root / "Build" / "Artifacts" / "Scripts" / "Game.scripts.dll", "dll");
    WriteFile(
        root / "Build" / "Artifacts" / "Scripts" /
            "Game.scripts.runtimeconfig.json",
        R"({"runtimeOptions":{"tfm":"net10.0"}})");

    const auto result = service.LoadPackage(MakePackageRequest(root));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        CapabilityManifestInvalid);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, MissingCapabilityEntryDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto request =
        MakeValidPackageRequest("saga_script_lifecycle_missing_capability_entry");
    WriteFile(
        request.packageRoot / "Manifests" / "script_capabilities.json",
        ScriptCapabilityManifestJson("script://game/missing"));

    const auto result = service.LoadPackage(request);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        CapabilityManifestMismatch);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, DuplicateCapabilityEntriesDoNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto request =
        MakeValidPackageRequest("saga_script_lifecycle_duplicate_capability_entry");
    WriteFile(
        request.packageRoot / "Manifests" / "script_capabilities.json",
        "{\n"
        "  \"schemaVersion\": 1,\n"
        "  \"scripts\": [\n"
        "    {\n"
        "      \"scriptId\": \"script://game/player\",\n"
        "      \"authority\": \"ServerOnly\",\n"
        "      \"packageDestinationIntent\": \"server\",\n"
        "      \"requestedCapabilities\": [],\n"
        "      \"grantedCapabilities\": []\n"
        "    },\n"
        "    {\n"
        "      \"scriptId\": \"script://game/player\",\n"
        "      \"authority\": \"ServerOnly\",\n"
        "      \"packageDestinationIntent\": \"server\",\n"
        "      \"requestedCapabilities\": [],\n"
        "      \"grantedCapabilities\": []\n"
        "    }\n"
        "  ]\n"
        "}\n");

    const auto result = service.LoadPackage(request);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        CapabilityManifestMismatch);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, CapabilityAuthorityMismatchDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto request =
        MakeValidPackageRequest("saga_script_lifecycle_capability_authority_mismatch");
    WriteFile(
        request.packageRoot / "Manifests" / "script_capabilities.json",
        ScriptCapabilityManifestJson(
            "script://game/player",
            "server",
            "ClientOnly"));

    const auto result = service.LoadPackage(request);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        CapabilityManifestMismatch);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, CapabilityDestinationMismatchDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto request =
        MakeValidPackageRequest("saga_script_lifecycle_capability_destination_mismatch");
    WriteFile(
        request.packageRoot / "Manifests" / "script_capabilities.json",
        ScriptCapabilityManifestJson(
            "script://game/player",
            "client",
            "ServerOnly"));

    const auto result = service.LoadPackage(request);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        CapabilityManifestMismatch);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, RequestedCapabilityMismatchDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto request = MakeValidPackageRequest(
        "saga_script_lifecycle_requested_capability_mismatch",
        "server",
        "ServerOnly",
        kDllArtifactHash,
        "net10.0",
        "[\"Gameplay.Inventory.Write\"]");
    WriteFile(
        request.packageRoot / "Manifests" / "script_capabilities.json",
        ScriptCapabilityManifestJson());

    const auto result = service.LoadPackage(request);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        CapabilityManifestMismatch);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, GrantedCapabilityMismatchDoesNotReachHost)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto request = MakeValidPackageRequest(
        "saga_script_lifecycle_granted_capability_mismatch",
        "server",
        "ServerOnly",
        kDllArtifactHash,
        "net10.0",
        "[]",
        "[\"Gameplay.Inventory.Write\"]");
    WriteFile(
        request.packageRoot / "Manifests" / "script_capabilities.json",
        ScriptCapabilityManifestJson());

    const auto result = service.LoadPackage(request);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics[0].diagnostic.code.value,
        CapabilityManifestMismatch);
    EXPECT_EQ(host.loadPackageCallCount, 0u);
}

TEST(ScriptLifecycleServiceTests, SharedDestinationIsAcceptedForClientAndServer)
{
    MockScriptHost serverHost;
    ScriptPackageValidationOptions serverOptions;
    serverOptions.expectedPackageDestination = "server";
    ScriptLifecycleService serverService(serverHost, {}, serverOptions);

    const auto serverResult = serverService.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_shared_server",
        "shared",
        "SharedPure"));

    ASSERT_TRUE(serverResult.Succeeded());
    EXPECT_EQ(serverHost.loadPackageCallCount, 1u);

    MockScriptHost clientHost;
    ScriptPackageValidationOptions clientOptions;
    clientOptions.expectedPackageDestination = "client";
    ScriptLifecycleService clientService(clientHost, {}, clientOptions);

    const auto clientResult = clientService.LoadPackage(MakeValidPackageRequest(
        "saga_script_lifecycle_shared_client",
        "shared",
        "SharedPure"));

    ASSERT_TRUE(clientResult.Succeeded());
    EXPECT_EQ(clientHost.loadPackageCallCount, 1u);
}

TEST(ScriptLifecycleServiceTests, LifecycleFailureProducesDeterministicDiagnostic)
{
    MockScriptHost host;
    host.failingMethod = ScriptLifecycleMethod::OnStart;
    ScriptLifecycleService service(host);

    const auto result = service.StartInstance(ScriptInstanceHandle{7});

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, LifecycleFailed);
    ASSERT_EQ(host.lifecycleCalls.size(), 2u);
    EXPECT_EQ(host.lifecycleCalls[0].method, ScriptLifecycleMethod::OnCreate);
    EXPECT_EQ(host.lifecycleCalls[1].method, ScriptLifecycleMethod::OnStart);
}

TEST(ScriptLifecycleServiceTests, InvalidInstanceHandleIsRejectedSafely)
{
    MockScriptHost host;
    ScriptLifecycleService service(host);

    const auto result = service.UpdateInstance(ScriptInstanceHandle{}, 1.0);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnostic.code.value, InvalidInstanceHandle);
    EXPECT_TRUE(host.lifecycleCalls.empty());
}

TEST(ScriptLifecycleServiceTests, EngineScriptingBoundaryHasNoEditorOrRuntimeHostDependency)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::vector<std::filesystem::path> files = {
        root / "Engine" / "Public" / "SagaEngine" / "Scripting" /
            "ISagaScriptHost.hpp",
        root / "Engine" / "Public" / "SagaEngine" / "Scripting" /
            "ScriptLifecycleService.hpp",
        root / "Engine" / "Private" / "SagaEngine" / "Scripting" /
            "ScriptLifecycleService.cpp",
        root / "Engine" / "Public" / "SagaEngine" / "Scripting" /
            "ScriptPackageValidator.hpp",
        root / "Engine" / "Private" / "SagaEngine" / "Scripting" /
            "ScriptPackageValidator.cpp",
    };

    for (const auto& file : files)
    {
        ASSERT_TRUE(std::filesystem::exists(file)) << file.generic_string();
        EXPECT_FALSE(ContainsForbiddenToken(ReadText(file)))
            << "Forbidden dependency token found in " << file.generic_string();
    }
}
