/// @file FirstPlayableGateTests.cpp
/// @brief Focused release-candidate gate policy tests.
#include "FirstPlayable/FirstPlayableGate.h"
#include "FirstPlayable/FirstPlayableEvidenceBundle.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <algorithm>

namespace
{
namespace fs = std::filesystem;
using namespace SagaProduct;
using Json = nlohmann::json;

void Write(const fs::path& path, const Json& value)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path); output << value.dump(2) << '\n';
}
void Text(const fs::path& path, const std::string& value)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path) << value;
}
Json Read(const fs::path& path)
{
    std::ifstream input(path); Json value; input >> value; return value;
}
fs::path Fixture(const char* name)
{
    const auto root = fs::temp_directory_path() / name;
    fs::remove_all(root);
    Write(root / "project" / "StarterArena.sagaproj", {{"projectId", "starter-arena"}});
    Write(root / "project" / "Scenes" / "arena.scene.json", Json::object());
    Text(root / "project" / "Scripts" / "GameRules.cs", "class GameRules {}\n");
    Write(root / "project" / "Input" / "playable.synthetic-input.json", Json::object());
    std::string claims;
    for (const auto& claim : FirstPlayablePublicClaimAudit::CanonicalNonClaims())
        claims += "- " + claim + "\n";
    Text(root / "project" / "README.md", claims);
    Text(root / "project" / "ACCEPTANCE.md", "Exact evidence only.\n");
    Text(root / "project" / "KNOWN_LIMITATIONS.md", "Bounded evidence.\n");
    fs::create_directories(root / ".git");
    return root;
}

TEST(FirstPlayableWorkspacePolicyTest, FreshExternalOutputPassesAndDetectsDrift)
{
    const auto root = Fixture("saga_gate_workspace");
    const auto output = fs::temp_directory_path() / "saga_gate_workspace_output";
    fs::remove_all(output);
    auto session = FirstPlayableWorkspacePolicy::Begin(
        root / "project" / "StarterArena.sagaproj", output,
        output / "first_playable_summary.json");
    ASSERT_EQ(session.status, EvidenceStatus::Passed)
        << (session.diagnostics.empty() ? "no diagnostic" :
            session.diagnostics.front().code + ": " +
            session.diagnostics.front().message);
    auto result = FirstPlayableWorkspacePolicy::Complete(session);
    EXPECT_EQ(result.status, EvidenceStatus::Passed);
    EXPECT_EQ(result.sourceFiles.size(), 7u);

    const auto fresh = fs::temp_directory_path() / "saga_gate_workspace_fresh";
    fs::remove_all(fresh);
    session = FirstPlayableWorkspacePolicy::Begin(
        root / "project" / "StarterArena.sagaproj", fresh,
        fresh / "first_playable_summary.json");
    std::ofstream(root / "project" / "Scripts" / "GameRules.cs", std::ios::app) << "// drift\n";
    result = FirstPlayableWorkspacePolicy::Complete(session);
    EXPECT_EQ(result.status, EvidenceStatus::Failed);
    EXPECT_TRUE(result.sampleSourceModified);
}

TEST(FirstPlayableWorkspacePolicyTest, RejectsSourcePrivateAndNonemptyOutputs)
{
    const auto root = Fixture("saga_gate_workspace_rejections");
    const auto occupied = fs::temp_directory_path() / "saga_gate_occupied";
    fs::remove_all(occupied);
    Text(occupied / "stale.json", "{}");
    auto session = FirstPlayableWorkspacePolicy::Begin(
        root / "project" / "StarterArena.sagaproj", occupied,
        occupied / "first_playable_summary.json");
    EXPECT_EQ(session.status, EvidenceStatus::Incomplete);
    session = FirstPlayableWorkspacePolicy::Begin(
        root / "project" / "StarterArena.sagaproj", root / ".saga-private" / "out",
        root / ".saga-private" / "out" / "first_playable_summary.json");
    EXPECT_EQ(session.status, EvidenceStatus::Incomplete);
}

Json KeyboardReport()
{
    return {{"diagnostics", Json::array()},
        {"input", {{"source", "keyboard"}, {"status", "Passed"},
            {"realDeviceObserved", true}, {"framesWithInput", 2}}},
        {"simulation", {{"initialPosition", {{"x", 0.0}, {"y", 0.0}}},
            {"finalPosition", {{"x", 0.1}, {"y", 0.0}}}}}};
}

TEST(FirstPlayableManualEvidenceTest, MissingIsPendingAndValidReportPasses)
{
    EXPECT_EQ(FirstPlayableManualEvidence::Validate(std::nullopt).status,
              EvidenceStatus::PendingManualEvidence);
    const auto root = Fixture("saga_gate_keyboard");
    const auto path = root / "keyboard.json";
    Write(path, KeyboardReport());
    const auto result = FirstPlayableManualEvidence::Validate(path);
    EXPECT_EQ(result.status, EvidenceStatus::Passed);
    EXPECT_TRUE(result.realDeviceObserved);
    EXPECT_EQ(result.framesWithInput, 2u);
    EXPECT_EQ(result.source, "keyboard");
    ASSERT_TRUE(result.initialPosition.has_value());
    ASSERT_TRUE(result.finalPosition.has_value());
    EXPECT_NEAR(result.movementDistance, 0.1, 1e-9);
}

TEST(FirstPlayableManualEvidenceTest, RejectsFakeIdleAndMalformedReports)
{
    const auto root = Fixture("saga_gate_keyboard_invalid");
    auto report = KeyboardReport();
    report["input"]["realDeviceObserved"] = false;
    report["input"]["framesWithInput"] = 0;
    report["simulation"]["finalPosition"] = report["simulation"]["initialPosition"];
    const auto path = root / "keyboard.json";
    Write(path, report);
    const auto result = FirstPlayableManualEvidence::Validate(path);
    EXPECT_EQ(result.status, EvidenceStatus::Failed);
    EXPECT_GE(result.diagnostics.size(), 3u);
    const auto malformed = root / "array.json";
    Write(malformed, Json::array());
    EXPECT_EQ(FirstPlayableManualEvidence::Validate(malformed).status,
              EvidenceStatus::Failed);
    report = KeyboardReport();
    report["input"]["framesWithInput"] = -1;
    Write(path, report);
    EXPECT_EQ(FirstPlayableManualEvidence::Validate(path).status,
              EvidenceStatus::Failed);
}

TEST(FirstPlayablePublicClaimAuditTest, CanonicalClaimsPassAndPositiveClaimFails)
{
    const auto root = Fixture("saga_gate_claims");
    EXPECT_EQ(FirstPlayablePublicClaimAudit::Run(root / "project").status,
              EvidenceStatus::Passed);
    Text(root / "project" / "ACCEPTANCE.md", "StarterArena is production ready.\n");
    const auto result = FirstPlayablePublicClaimAudit::Run(root / "project");
    EXPECT_EQ(result.status, EvidenceStatus::Failed);
    EXPECT_TRUE(std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
        [](const auto& value) {
            return value.code == "ProductShell.FirstPlayable.ForbiddenPublicClaim";
        }));
}

TEST(FirstPlayablePublicClaimAuditTest, RejectsEveryBoundedPositiveClaimClass)
{
    const auto root = Fixture("saga_gate_claim_classes");
    for (const char* claim : {"The full editor is complete.",
        "The project creation workflow is complete.",
        "The Visual Blocks canvas is implemented.",
        "The Visual Blocks runtime graph is implemented.",
        "Generated C# from blocks is implemented.", "StarterArena is multiplayer ready.",
        "StarterArena is networking ready.", "Package install complete.",
        "Distribution ready.", "This is a complete game.", "Saga is MMO ready."})
    {
        Text(root / "project" / "ACCEPTANCE.md", claim);
        const auto result = FirstPlayablePublicClaimAudit::Run(root / "project");
        EXPECT_TRUE(std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
            [](const auto& value) {
                return value.code == "ProductShell.FirstPlayable.ForbiddenPublicClaim";
            })) << claim;
    }
}

TEST(FirstPlayableGateTest, PassedAutomationAcceptsWithKeyboardPending)
{
    RuntimeEvidenceRunResult runtime;
    runtime.prepared = true;
    for (auto profile : {RuntimeEvidenceProfile::StarterArenaSmoke,
        RuntimeEvidenceProfile::StarterArenaLifecycle,
        RuntimeEvidenceProfile::StarterArenaGameplay,
        RuntimeEvidenceProfile::StarterArenaVisibleSynthetic,
        RuntimeEvidenceProfile::StarterArenaVisibleGameplay})
        runtime.profiles.push_back({profile, EvidenceStatus::Passed});
    VisualBlocksDescriptorGenerationResult descriptor;
    descriptor.status = EvidenceStatus::Passed;
    FirstPlayableWorkspacePolicyResult workspace;
    workspace.status = workspace.sourceIntegrity = EvidenceStatus::Passed;
    FirstPlayablePublicClaimAuditResult claims;
    claims.status = EvidenceStatus::Passed;
    const auto gate = FirstPlayableGate::Evaluate(runtime, descriptor, workspace,
        FirstPlayableManualEvidenceResult{}, claims);
    EXPECT_EQ(gate.status,
              FirstPlayableGateStatus::AcceptedWithManualEvidencePending);
    EXPECT_EQ(gate.checks.size(), 4u);
}

TEST(FirstPlayableGateTest, PreflightFailureIsIncompleteWithoutProfileFailureClaim)
{
    RuntimeEvidenceRunResult runtime;
    VisualBlocksDescriptorGenerationResult descriptor;
    FirstPlayableWorkspacePolicyResult workspace;
    workspace.status = workspace.sourceIntegrity = EvidenceStatus::Incomplete;
    FirstPlayableDiagnostic policy;
    policy.code = "ProductShell.FirstPlayable.OutputRootNotEmpty";
    policy.message = "occupied";
    workspace.diagnostics.push_back(policy);
    FirstPlayablePublicClaimAuditResult claims;
    claims.status = EvidenceStatus::Passed;
    const auto gate = FirstPlayableGate::Evaluate(runtime, descriptor, workspace,
        FirstPlayableManualEvidenceResult{}, claims);
    EXPECT_EQ(gate.status, FirstPlayableGateStatus::Incomplete);
    EXPECT_EQ(gate.mandatoryProfiles, EvidenceStatus::Incomplete);
    EXPECT_EQ(gate.evidenceBundle, EvidenceStatus::Incomplete);
    EXPECT_TRUE(std::none_of(gate.diagnostics.begin(), gate.diagnostics.end(),
        [](const auto& value) {
            return value.code == "ProductShell.FirstPlayable.GateMandatoryProfileFailed";
        }));
}

TEST(FirstPlayableGateTest, HumanCaptureCheckControlsFinalGateStatus)
{
    RuntimeEvidenceRunResult runtime;
    runtime.prepared = true;
    for (auto profile : {RuntimeEvidenceProfile::StarterArenaSmoke,
        RuntimeEvidenceProfile::StarterArenaLifecycle,
        RuntimeEvidenceProfile::StarterArenaGameplay,
        RuntimeEvidenceProfile::StarterArenaVisibleSynthetic,
        RuntimeEvidenceProfile::StarterArenaVisibleGameplay})
        runtime.profiles.push_back({profile, EvidenceStatus::Passed});
    VisualBlocksDescriptorGenerationResult descriptor;
    descriptor.status = EvidenceStatus::Passed;
    FirstPlayableWorkspacePolicyResult workspace;
    workspace.status = workspace.sourceIntegrity = EvidenceStatus::Passed;
    FirstPlayablePublicClaimAuditResult claims;
    claims.status = EvidenceStatus::Passed;
    FirstPlayableManualEvidenceResult manual;
    manual.status = EvidenceStatus::Passed;
    const auto rejected = FirstPlayableGate::Evaluate(runtime, descriptor, workspace,
        manual, claims, FirstPlayableGateCheck{"human-capture", EvidenceStatus::Failed});
    EXPECT_EQ(rejected.status, FirstPlayableGateStatus::Rejected);
    ASSERT_EQ(rejected.checks.size(), 5u);
    const auto incomplete = FirstPlayableGate::Evaluate(runtime, descriptor, workspace,
        manual, claims, FirstPlayableGateCheck{"human-capture", EvidenceStatus::Incomplete});
    EXPECT_EQ(incomplete.status, FirstPlayableGateStatus::Incomplete);
}

TEST(FirstPlayableEvidenceBundleTest, ManifestListsSixProfilesWithRelativePaths)
{
    const auto output = fs::temp_directory_path() / "saga_gate_bundle";
    fs::remove_all(output);
    RuntimeEvidenceRunResult runtime;
    runtime.outputDirectory = output;
    for (auto profile : {RuntimeEvidenceProfile::StarterArenaSmoke,
        RuntimeEvidenceProfile::StarterArenaLifecycle,
        RuntimeEvidenceProfile::StarterArenaGameplay,
        RuntimeEvidenceProfile::StarterArenaVisibleSynthetic,
        RuntimeEvidenceProfile::StarterArenaVisibleGameplay})
    {
        RuntimeEvidenceProfileResult item;
        item.profile = profile;
        const auto directory = output / "profiles" / ToString(profile);
        item.reportPath = directory / "report.json";
        item.stdoutPath = directory / "stdout.txt";
        item.stderrPath = directory / "stderr.txt";
        Text(item.reportPath, "{}"); Text(item.stdoutPath, ""); Text(item.stderrPath, "");
        runtime.profiles.push_back(item);
    }
    VisualBlocksDescriptorGenerationResult descriptor;
    descriptor.reportPath = output / "profiles" / kVisualBlocksDescriptorProfileId /
        "report.json";
    Text(descriptor.reportPath, "{}");
    FirstPlayableGateResult gate;
    gate.workspacePolicy.projectRoot = output / "source";
    gate.nonClaims = FirstPlayablePublicClaimAudit::CanonicalNonClaims();
    FirstPlayableSourceFile source;
    source.relativePath = "StarterArena.sagaproj";
    source.beforeSha256 = source.afterSha256 = "hash"; source.unchanged = true;
    gate.workspacePolicy.sourceFiles.push_back(source);
    const auto summary = output / "first_playable_summary.json";
    const auto gatePath = output / "first_playable_gate.json";
    Text(summary, "{}"); Text(gatePath, "{}");
    const auto result = FirstPlayableEvidenceBundle::Write(
        output, runtime, descriptor, gate, summary, gatePath);
    ASSERT_EQ(result.status, EvidenceStatus::Passed);
    const Json manifest = Read(result.evidenceManifestPath);
    EXPECT_EQ(manifest["schemaVersion"], 2);
    EXPECT_EQ(manifest["profiles"].size(), 6u);
    EXPECT_EQ(manifest["manualEvidence"]["realKeyboard"]["status"],
              "PendingManualEvidence");
    for (const Json& artifact : manifest["artifacts"])
    {
        EXPECT_FALSE(artifact.value("path", "").starts_with("/"));
        EXPECT_TRUE(artifact.contains("role"));
    }
}

TEST(FirstPlayableBoundaryTest, GateSourcesExcludeForbiddenDependencies)
{
    const auto sourceRoot = fs::path(SAGA_SOURCE_ROOT) / "Tests/Evidence/FirstPlayable/Source";
    for (const char* file : {"FirstPlayableGate.cpp", "FirstPlayableWorkspacePolicy.cpp",
        "FirstPlayableManualEvidence.cpp", "FirstPlayablePublicClaimAudit.cpp",
        "FirstPlayableEvidenceBundle.cpp"})
    {
        std::ifstream input(sourceRoot / file);
        const std::string source((std::istreambuf_iterator<char>(input)), {});
        for (const char* forbidden : {"SDE::", ".sde", "Prism", "ClientHost",
            "SagaEngine/ServerAuthority", "SagaEngine/Replication",
            "InputCommand", "Replication", "Prediction", "QProcess"})
            EXPECT_EQ(source.find(forbidden), std::string::npos) << file << ": " << forbidden;
    }
}
} // namespace
