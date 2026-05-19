/// @file ArtifactPackageBuildContractsTests.cpp
/// @brief Verifies shared artifact, package, build, and publish contracts.

#include <SagaShared/Artifacts/ArtifactManifest.hpp>
#include <SagaShared/Build/BuildProfile.hpp>
#include <SagaShared/Build/BuildReport.hpp>
#include <SagaShared/Packages/PackageManifest.hpp>
#include <SagaShared/Packages/PackageValidationResult.hpp>
#include <SagaShared/Publish/PublishReport.hpp>

#include <gtest/gtest.h>

using SagaShared::Artifacts::ArtifactKind;
using SagaShared::Artifacts::ArtifactManifest;
using SagaShared::Artifacts::ArtifactRef;
using SagaShared::Artifacts::ArtifactStatus;
using SagaShared::Build::BuildProfile;
using SagaShared::Build::BuildReport;
using SagaShared::Build::BuildStatus;
using SagaShared::Build::BuildStepKind;
using SagaShared::Build::BuildStepResult;
using SagaShared::Packages::PackageKind;
using SagaShared::Packages::PackageManifest;
using SagaShared::Packages::PackageValidationResult;
using SagaShared::Publish::PublishBlocker;
using SagaShared::Publish::PublishBlockerKind;
using SagaShared::Publish::PublishReport;
using SagaShared::Publish::PublishStatus;

TEST(SharedArtifactContractsTests, ArtifactRefCarriesStableMetadata)
{
    ArtifactRef ref;
    ref.id.value = "artifact://graphs/combat";
    ref.kind = ArtifactKind::Graph;
    ref.status = ArtifactStatus::Generated;
    ref.path = "Build/Artifacts/Graphs/combat.graph.json";
    ref.hash.algorithm = "sha256";
    ref.hash.value = "abc123";
    ref.schemaId = "schema://graph/v1";
    ref.sourceResourceId = "graph://combat";
    ref.buildProfile = "dev-client";
    ref.targetPlatform = "linux";
    ref.producingStepId = "compile-graphs";
    ref.diagnostics.Add(SagaShared::Diagnostics::DiagnosticSeverity::Warning);
    ref.metadata["domain"] = "gameplay";

    EXPECT_EQ(ref.id.value, "artifact://graphs/combat");
    EXPECT_EQ(ref.kind, ArtifactKind::Graph);
    EXPECT_EQ(ref.status, ArtifactStatus::Generated);
    EXPECT_EQ(ref.path, "Build/Artifacts/Graphs/combat.graph.json");
    EXPECT_EQ(ref.hash.algorithm, "sha256");
    EXPECT_EQ(ref.hash.value, "abc123");
    EXPECT_EQ(ref.schemaId, "schema://graph/v1");
    EXPECT_EQ(ref.sourceResourceId, "graph://combat");
    EXPECT_EQ(ref.buildProfile, "dev-client");
    EXPECT_EQ(ref.targetPlatform, "linux");
    EXPECT_EQ(ref.producingStepId, "compile-graphs");
    EXPECT_EQ(ref.diagnostics.warningCount, 1u);
    EXPECT_EQ(ref.metadata.at("domain"), "gameplay");
}

TEST(SharedArtifactContractsTests, ManifestCarriesArtifactsAndDependencies)
{
    ArtifactRef graph;
    graph.id.value = "artifact://graphs/combat";
    graph.kind = ArtifactKind::Graph;

    ArtifactRef dependency;
    dependency.id.value = "artifact://schema/graph";
    dependency.kind = ArtifactKind::Schema;

    ArtifactManifest manifest;
    manifest.schemaVersion = 1;
    manifest.manifestId = "manifest://artifacts/dev-client";
    manifest.artifacts.push_back(graph);
    manifest.artifacts.push_back(dependency);
    manifest.dependencies.push_back(
        {graph.id, dependency.id, "schema", true});
    manifest.diagnostics.Add(SagaShared::Diagnostics::DiagnosticSeverity::Info);

    EXPECT_EQ(manifest.schemaVersion, 1u);
    EXPECT_EQ(manifest.manifestId, "manifest://artifacts/dev-client");
    ASSERT_EQ(manifest.artifacts.size(), 2u);
    EXPECT_EQ(manifest.artifacts[0].kind, ArtifactKind::Graph);
    ASSERT_EQ(manifest.dependencies.size(), 1u);
    EXPECT_EQ(manifest.dependencies[0].fromArtifact.value,
              "artifact://graphs/combat");
    EXPECT_EQ(manifest.dependencies[0].toArtifact.value,
              "artifact://schema/graph");
    EXPECT_TRUE(manifest.dependencies[0].required);
    EXPECT_EQ(manifest.diagnostics.infoCount, 1u);
}

TEST(SharedPackageContractsTests, PackageManifestCarriesArtifactsAndCompatibility)
{
    ArtifactRef graphManifest;
    graphManifest.id.value = "artifact://manifests/graphs";
    graphManifest.kind = ArtifactKind::Manifest;

    ArtifactRef graphArtifact;
    graphArtifact.id.value = "artifact://graphs/combat";
    graphArtifact.kind = ArtifactKind::Graph;

    PackageManifest manifest;
    manifest.schemaVersion = 1;
    manifest.packageId.value = "package://dev-client";
    manifest.packageKind = PackageKind::Client;
    manifest.packageName = "Dev Client";
    manifest.version.major = 0;
    manifest.version.minor = 0;
    manifest.version.patch = 8;
    manifest.buildProfile = "dev-client";
    manifest.targetPlatform = "linux";
    manifest.artifacts.push_back(graphArtifact);
    manifest.graphManifests.push_back(graphManifest);
    manifest.compatibility.runtimeVersion = "0.0.8";
    manifest.compatibility.targetPlatform = "linux";

    EXPECT_EQ(manifest.packageId.value, "package://dev-client");
    EXPECT_EQ(manifest.packageKind, PackageKind::Client);
    EXPECT_EQ(manifest.packageName, "Dev Client");
    EXPECT_EQ(manifest.version.patch, 8u);
    ASSERT_EQ(manifest.artifacts.size(), 1u);
    EXPECT_EQ(manifest.artifacts[0].kind, ArtifactKind::Graph);
    ASSERT_EQ(manifest.graphManifests.size(), 1u);
    EXPECT_EQ(manifest.graphManifests[0].id.value,
              "artifact://manifests/graphs");
    EXPECT_EQ(manifest.compatibility.runtimeVersion, "0.0.8");
}

TEST(SharedPackageContractsTests, ValidationResultUsesDiagnosticPayloads)
{
    PackageValidationResult result;
    result.packageId.value = "package://dev-client";
    result.valid = false;
    result.recoverable = true;

    SagaShared::Diagnostics::DiagnosticPayload diagnostic;
    diagnostic.severity =
        SagaShared::Diagnostics::DiagnosticSeverity::PublishBlocking;
    diagnostic.code.value = "Package.Manifest.MissingArtifact";
    result.diagnostics.push_back(diagnostic);
    result.summary.Add(diagnostic);

    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.recoverable);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code.value,
              "Package.Manifest.MissingArtifact");
    EXPECT_EQ(result.summary.publishBlockingCount, 1u);
}

TEST(SharedBuildContractsTests, BuildProfileAndReportCarryStepsAndOutputs)
{
    BuildProfile profile;
    profile.profileId.value = "dev-client";
    profile.displayName = "Dev Client";
    profile.targetPlatform.id = "linux";
    profile.outputPackageKind = PackageKind::Client;
    profile.configuration.validationStrictness = "warnings";

    ArtifactRef output;
    output.id.value = "artifact://graphs/combat";
    output.kind = ArtifactKind::Graph;

    BuildStepResult step;
    step.stepId = "compile-graphs";
    step.stepKind = BuildStepKind::CompileSDE;
    step.status = BuildStatus::SucceededWithWarnings;
    step.outputs.push_back(output);
    step.diagnosticSummary.Add(
        SagaShared::Diagnostics::DiagnosticSeverity::Warning);

    BuildReport report;
    report.buildId.value = "build-42";
    report.profileId = profile.profileId;
    report.targetPlatform = profile.targetPlatform;
    report.status = BuildStatus::SucceededWithWarnings;
    report.steps.push_back(step);
    report.artifactOutputs.push_back(output);
    report.diagnosticSummary.Add(
        SagaShared::Diagnostics::DiagnosticSeverity::Warning);

    EXPECT_EQ(profile.profileId.value, "dev-client");
    EXPECT_EQ(profile.outputPackageKind, PackageKind::Client);
    EXPECT_EQ(report.buildId.value, "build-42");
    EXPECT_EQ(report.status, BuildStatus::SucceededWithWarnings);
    ASSERT_EQ(report.steps.size(), 1u);
    EXPECT_EQ(report.steps[0].stepKind, BuildStepKind::CompileSDE);
    EXPECT_EQ(report.steps[0].diagnosticSummary.warningCount, 1u);
    ASSERT_EQ(report.artifactOutputs.size(), 1u);
    EXPECT_EQ(report.artifactOutputs[0].id.value, "artifact://graphs/combat");
}

TEST(SharedPublishContractsTests, PublishReportCarriesReadinessAndBlockers)
{
    ArtifactRef staleArtifact;
    staleArtifact.id.value = "artifact://assets/tree";
    staleArtifact.kind = ArtifactKind::Asset;
    staleArtifact.status = ArtifactStatus::Stale;

    PublishBlocker blocker;
    blocker.kind = PublishBlockerKind::StaleArtifact;
    blocker.message = "Cooked asset is stale.";
    blocker.packageId.value = "package://dev-client";
    blocker.affectedResourceIds.push_back("asset://tree");
    blocker.affectedArtifacts.push_back(staleArtifact);

    PublishReport report;
    report.reportId = "publish-report-42";
    report.buildId.value = "build-42";
    report.packageId.value = "package://dev-client";
    report.readiness.status = PublishStatus::Blocked;
    report.readiness.blockers.push_back(blocker);
    report.diagnosticSummary.Add(
        SagaShared::Diagnostics::DiagnosticSeverity::PublishBlocking);

    EXPECT_EQ(report.reportId, "publish-report-42");
    EXPECT_EQ(report.buildId.value, "build-42");
    EXPECT_EQ(report.packageId.value, "package://dev-client");
    EXPECT_FALSE(report.readiness.IsReady());
    ASSERT_EQ(report.readiness.blockers.size(), 1u);
    EXPECT_EQ(report.readiness.blockers[0].kind,
              PublishBlockerKind::StaleArtifact);
    ASSERT_EQ(report.readiness.blockers[0].affectedArtifacts.size(), 1u);
    EXPECT_EQ(report.readiness.blockers[0].affectedArtifacts[0].status,
              ArtifactStatus::Stale);
    EXPECT_EQ(report.diagnosticSummary.publishBlockingCount, 1u);
}
