/// @file SagaScriptContractsTests.cpp
/// @brief Verifies shared SagaScript contract descriptors.

#include <SagaShared/Artifacts/ArtifactKind.hpp>
#include <SagaShared/Diagnostics/DiagnosticCategory.hpp>
#include <SagaShared/Diagnostics/DiagnosticSeverity.hpp>
#include <SagaShared/Diagnostics/DiagnosticSource.hpp>
#include <SagaShared/Scripting/ScriptManifest.hpp>

#include <gtest/gtest.h>

using SagaShared::Artifacts::ArtifactKind;
using SagaShared::Diagnostics::DiagnosticCategory;
using SagaShared::Diagnostics::DiagnosticSeverity;
using SagaShared::Diagnostics::DiagnosticSource;
using SagaShared::Scripting::ScriptArtifactManifest;
using SagaShared::Scripting::ScriptAuthorityContext;
using SagaShared::Scripting::ScriptAuthorityDescriptor;
using SagaShared::Scripting::ScriptBindingDescriptor;
using SagaShared::Scripting::ScriptBindingManifest;
using SagaShared::Scripting::ScriptCapabilityDomain;
using SagaShared::Scripting::ScriptCapabilityGrant;
using SagaShared::Scripting::ScriptCapabilityManifest;
using SagaShared::Scripting::ScriptCapabilityRequest;
using SagaShared::Scripting::ScriptExecutionDomain;
using SagaShared::Scripting::ScriptPredictionSafety;
using SagaShared::Scripting::ScriptSecurityBoundary;
using SagaShared::Scripting::ScriptSideEffect;

TEST(SagaScriptContractsTests, BindingDescriptorCarriesAuthorityAndCapabilities)
{
    ScriptBindingDescriptor binding;
    binding.bindingId = "binding://inventory/give-item";
    binding.scriptId.value = "script://gameplay/inventory";
    binding.className = "Game.InventoryScripts";
    binding.methodName = "GiveItem";
    binding.blockCategory = "Inventory";
    binding.blockName = "Give Item";
    binding.parameters.push_back({"player", "PlayerRef", "", true});
    binding.parameters.push_back({"item", "ItemId", "", true});
    binding.parameters.push_back({"count", "int", "1", true});

    ScriptAuthorityDescriptor authority;
    authority.authorityContext = ScriptAuthorityContext::ServerOnly;
    authority.executionDomain = ScriptExecutionDomain::ServerRuntime;
    authority.predictionSafety = ScriptPredictionSafety::PredictionUnsafe;
    authority.securityBoundary = ScriptSecurityBoundary::TrustedServerOnly;
    authority.sideEffects.push_back(ScriptSideEffect::WriteAuthoritativeState);
    authority.sideEffects.push_back(ScriptSideEffect::WritePersistentState);
    authority.sideEffects.push_back(ScriptSideEffect::WriteReplicatedState);
    authority.replicationEffect = "ReplicateInventory";
    authority.persistenceEffect = "TransactionalWrite";
    binding.authority = authority;

    ScriptCapabilityRequest request;
    request.domain = ScriptCapabilityDomain::TrustedServerOnly;
    request.capabilityId = "Gameplay.Inventory.Write";
    request.required = true;
    request.rationale = "Grant inventory rewards from server authority.";
    binding.capabilities.requested.push_back(request);

    ScriptCapabilityGrant grant;
    grant.domain = ScriptCapabilityDomain::TrustedServerOnly;
    grant.capabilityId = "Gameplay.Inventory.Write";
    grant.granted = true;
    grant.policyId = "server-authority";
    binding.capabilities.granted.push_back(grant);

    binding.artifact.scriptId = binding.scriptId.value;
    binding.artifact.artifact.id.value = "artifact://scripts/inventory";
    binding.artifact.artifact.kind = ArtifactKind::Script;
    binding.artifact.packageDestination = "server";
    binding.sourceLocation.sourcePath = "Scripts/Inventory.cs";
    binding.sourceLocation.line = 42;

    EXPECT_EQ(binding.bindingId, "binding://inventory/give-item");
    EXPECT_EQ(binding.scriptId.value, "script://gameplay/inventory");
    EXPECT_EQ(binding.methodName, "GiveItem");
    ASSERT_EQ(binding.parameters.size(), 3u);
    EXPECT_EQ(binding.parameters[2].defaultValue, "1");
    EXPECT_EQ(binding.authority.authorityContext, ScriptAuthorityContext::ServerOnly);
    EXPECT_EQ(binding.authority.executionDomain, ScriptExecutionDomain::ServerRuntime);
    EXPECT_EQ(binding.authority.securityBoundary, ScriptSecurityBoundary::TrustedServerOnly);
    ASSERT_EQ(binding.authority.sideEffects.size(), 3u);
    EXPECT_EQ(binding.authority.replicationEffect, "ReplicateInventory");
    ASSERT_EQ(binding.capabilities.requested.size(), 1u);
    EXPECT_EQ(binding.capabilities.requested[0].capabilityId,
              "Gameplay.Inventory.Write");
    ASSERT_EQ(binding.capabilities.granted.size(), 1u);
    EXPECT_TRUE(binding.capabilities.granted[0].granted);
    EXPECT_EQ(binding.artifact.artifact.kind, ArtifactKind::Script);
    EXPECT_EQ(binding.artifact.packageDestination, "server");
    EXPECT_EQ(binding.sourceLocation.sourcePath, "Scripts/Inventory.cs");
}

TEST(SagaScriptContractsTests, BindingManifestCarriesDiagnosticsAndGeneratedOrigin)
{
    ScriptBindingDescriptor binding;
    binding.bindingId = "binding://hud/show-message";
    binding.scriptId.value = "script://ui/hud";
    binding.authority.authorityContext = ScriptAuthorityContext::ClientOnly;
    binding.authority.executionDomain = ScriptExecutionDomain::ClientRuntime;
    binding.authority.sideEffects.push_back(ScriptSideEffect::WriteLocalState);
    binding.generatedOrigin.originId = "generated://graphs/hud";
    binding.generatedOrigin.sourceResourceId = "graph://hud";
    binding.generatedOrigin.sourceHash = "sha256:graph";
    binding.generatedOrigin.generatedPath = "Build/Generated/Hud.g.cs";
    binding.generatedOrigin.editableDetachedCopy = false;

    ScriptBindingManifest manifest;
    manifest.schemaVersion = 1;
    manifest.manifestId = "manifest://scripts/bindings/dev-client";
    manifest.sourceHash = "sha256:scripts";
    manifest.toolchainId = "sagascript";
    manifest.toolchainVersion = "0.0.8-dev";
    manifest.bindings.push_back(binding);

    SagaShared::Scripting::ScriptDiagnosticPayload diagnostic;
    diagnostic.scriptId = binding.scriptId.value;
    diagnostic.bindingId = binding.bindingId;
    diagnostic.diagnostic.severity = DiagnosticSeverity::Warning;
    diagnostic.diagnostic.category = DiagnosticCategory::Script;
    diagnostic.diagnostic.source = DiagnosticSource::ScriptingToolchain;
    diagnostic.diagnostic.code.value = "Script.Binding.GeneratedEvaluation";
    manifest.diagnostics.push_back(diagnostic);
    manifest.diagnosticSummary.Add(diagnostic.diagnostic);

    EXPECT_EQ(manifest.schemaVersion, 1u);
    EXPECT_EQ(manifest.manifestId, "manifest://scripts/bindings/dev-client");
    ASSERT_EQ(manifest.bindings.size(), 1u);
    EXPECT_EQ(manifest.bindings[0].generatedOrigin.sourceResourceId, "graph://hud");
    EXPECT_FALSE(manifest.bindings[0].generatedOrigin.editableDetachedCopy);
    ASSERT_EQ(manifest.diagnostics.size(), 1u);
    EXPECT_EQ(manifest.diagnostics[0].diagnostic.category, DiagnosticCategory::Script);
    EXPECT_EQ(manifest.diagnosticSummary.warningCount, 1u);
}

TEST(SagaScriptContractsTests, ArtifactAndCapabilityManifestsSeparatePolicyFromArtifact)
{
    ScriptArtifactManifest artifacts;
    artifacts.schemaVersion = 1;
    artifacts.manifestId = "manifest://scripts/artifacts/server";
    artifacts.sourceHash = "sha256:scripts";
    artifacts.toolchainId = "sagascript";
    artifacts.toolchainVersion = "0.0.8-dev";

    SagaShared::Scripting::ScriptArtifactRef scriptArtifact;
    scriptArtifact.scriptId = "script://gameplay/inventory";
    scriptArtifact.artifact.id.value = "artifact://scripts/inventory/server";
    scriptArtifact.artifact.kind = ArtifactKind::Script;
    scriptArtifact.artifact.path = "Build/Scripts/Server/Inventory.dll";
    scriptArtifact.packageDestination = "server";
    scriptArtifact.entryPoint = "Game.InventoryScripts";
    artifacts.artifacts.push_back(scriptArtifact);

    ScriptCapabilityManifest capabilities;
    capabilities.schemaVersion = 1;
    capabilities.manifestId = "manifest://scripts/capabilities/server";
    capabilities.sourceHash = artifacts.sourceHash;
    capabilities.policyId = "server-authority";

    ScriptCapabilityRequest request;
    request.domain = ScriptCapabilityDomain::EngineInternal;
    request.capabilityId = "Engine.Internal.UnsafeWorldMutation";
    request.required = true;
    capabilities.capabilities.requested.push_back(request);

    ScriptCapabilityGrant denial;
    denial.domain = ScriptCapabilityDomain::EngineInternal;
    denial.capabilityId = request.capabilityId;
    denial.granted = false;
    denial.policyId = "server-authority";
    denial.reason = "EngineInternal is not granted to project scripts.";
    capabilities.capabilities.granted.push_back(denial);

    SagaShared::Scripting::ScriptDiagnosticPayload diagnostic;
    diagnostic.scriptId = scriptArtifact.scriptId;
    diagnostic.artifactId = scriptArtifact.artifact.id.value;
    diagnostic.diagnostic.severity = DiagnosticSeverity::Security;
    diagnostic.diagnostic.category = DiagnosticCategory::Security;
    diagnostic.diagnostic.source = DiagnosticSource::ScriptingToolchain;
    diagnostic.diagnostic.code.value = "Script.Capability.EngineInternalDenied";
    capabilities.diagnostics.push_back(diagnostic);
    capabilities.diagnosticSummary.Add(diagnostic.diagnostic);

    ASSERT_EQ(artifacts.artifacts.size(), 1u);
    EXPECT_EQ(artifacts.artifacts[0].packageDestination, "server");
    EXPECT_EQ(artifacts.artifacts[0].artifact.path,
              "Build/Scripts/Server/Inventory.dll");
    EXPECT_EQ(capabilities.policyId, "server-authority");
    ASSERT_EQ(capabilities.capabilities.requested.size(), 1u);
    EXPECT_EQ(capabilities.capabilities.requested[0].domain,
              ScriptCapabilityDomain::EngineInternal);
    ASSERT_EQ(capabilities.capabilities.granted.size(), 1u);
    EXPECT_FALSE(capabilities.capabilities.granted[0].granted);
    EXPECT_EQ(capabilities.diagnosticSummary.securityCount, 1u);
}
