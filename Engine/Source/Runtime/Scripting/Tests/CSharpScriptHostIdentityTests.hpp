// SPDX-License-Identifier: MPL-2.0
/// @file CSharpScriptHostIdentityTests.hpp
/// @brief Active-identity, stale-context, and atomic-publication host regressions.

#pragma once

using SagaEngine::Scripting::ScriptHostDiagnostics::InvalidInstanceHandle;
using SagaEngine::Scripting::ScriptHostDiagnostics::InstanceCreationFailed;

[[nodiscard]] const std::filesystem::path& ManagedLifecycleLogPath()
{
    static const auto path =
        std::filesystem::temp_directory_path() / "saga_csharp_host_lifecycle.log";
    return path;
}

const bool kManagedLifecycleLogEnvironmentInitialized = []
{
    SetEnvironment("SAGA_CSHARP_HOST_TEST_LOG", ManagedLifecycleLogPath());
    return true;
}();

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

TEST(CSharpScriptHostTests, VerifiedGenerationRejectsBehaviorManifestV1)
{
    auto fixture = CreateVerifiedGeneration("saga_verified_generation_behavior_v1");
    auto manifest = ReadJson(fixture.request.behaviorManifest);
    manifest["schemaVersion"] = 1;
    manifest["behaviors"][0].erase("stateAccesses");
    RewriteVerifiedJson(fixture.request.behaviorManifest, manifest);
    CSharpScriptHost host(HostOptions());
    const auto result = host.LoadVerifiedGeneration(fixture.request);
    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(result.diagnostics.front().diagnostic.code.value,
        SagaEngine::Scripting::ScriptHostDiagnostics::ManifestInvalid);
}

TEST(CSharpScriptHostTests, VerifiedGenerationRejectsMalformedOrDuplicateStateAccess)
{
    auto missingFixture = CreateVerifiedGeneration(
        "saga_verified_generation_state_access_missing");
    auto missing = ReadJson(missingFixture.request.behaviorManifest);
    missing["behaviors"][0].erase("stateAccesses");
    RewriteVerifiedJson(missingFixture.request.behaviorManifest, missing);
    CSharpScriptHost missingHost(HostOptions());
    const auto missingResult = missingHost.LoadVerifiedGeneration(missingFixture.request);
    ASSERT_FALSE(missingResult.Succeeded());
    EXPECT_EQ(missingResult.diagnostics.front().diagnostic.code.value,
        SagaEngine::Scripting::ScriptHostDiagnostics::ManifestInvalid);

    auto duplicateFixture = CreateVerifiedGeneration(
        "saga_verified_generation_state_access_duplicate");
    auto behaviors = ReadJson(duplicateFixture.request.behaviorManifest);
    auto capabilities = ReadJson(duplicateFixture.request.capabilityManifest);
    capabilities["capabilities"].push_back("Gameplay.State.ReadWrite");
    behaviors["behaviors"][0]["capabilities"].push_back("Gameplay.State.ReadWrite");
    const nlohmann::json access{{"operation", "SetBool"}, {"valueType", "Bool"},
                                {"key", "light.enabled"}};
    behaviors["behaviors"][0]["stateAccesses"] = nlohmann::json::array({access, access});
    RewriteVerifiedJson(duplicateFixture.request.behaviorManifest, behaviors);
    RewriteVerifiedJson(duplicateFixture.request.capabilityManifest, capabilities);
    CSharpScriptHost duplicateHost(HostOptions());
    const auto duplicateResult =
        duplicateHost.LoadVerifiedGeneration(duplicateFixture.request);
    ASSERT_FALSE(duplicateResult.Succeeded());
    EXPECT_EQ(duplicateResult.diagnostics.front().diagnostic.code.value,
        SagaEngine::Scripting::ScriptHostDiagnostics::ManifestInvalid);
}
