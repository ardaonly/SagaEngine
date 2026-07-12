/// @file FirstPlayableWorkflow.cpp
/// @brief Product Shell first-playable release-candidate evidence workflow.
#include "FirstPlayableWorkflow.h"

#include "FirstPlayableEvidenceBundle.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <ostream>

namespace SagaProduct
{
namespace
{
using Json = nlohmann::json;
Json DiagnosticJson(const FirstPlayableDiagnostic& diagnostic)
{
    Json value = {{"severity", ToString(diagnostic.severity)},
        {"code", diagnostic.code}, {"message", diagnostic.message}};
    if (diagnostic.profileId) value["profileId"] = *diagnostic.profileId;
    if (diagnostic.path) value["path"] = diagnostic.path->string();
    if (diagnostic.actionHint) value["actionHint"] = *diagnostic.actionHint;
    if (diagnostic.blockId) value["blockId"] = *diagnostic.blockId;
    if (diagnostic.fieldPath) value["fieldPath"] = *diagnostic.fieldPath;
    return value;
}
void Merge(RuntimeEvidenceCapabilities& target, const RuntimeEvidenceCapabilities& source)
{
    auto merge = [](EvidenceStatus& current, EvidenceStatus incoming) {
        if (incoming == EvidenceStatus::Passed ||
            (current == EvidenceStatus::NotPresent && incoming != EvidenceStatus::NotPresent) ||
            (current == EvidenceStatus::NotRequested && incoming != EvidenceStatus::NotRequested))
            current = incoming;
    };
    merge(target.project, source.project); merge(target.scene, source.scene);
    merge(target.runtimeSmoke, source.runtimeSmoke);
    merge(target.scriptMetadata, source.scriptMetadata);
    merge(target.invocation, source.invocation); merge(target.lifecycle, source.lifecycle);
    merge(target.gameplay, source.gameplay); merge(target.input, source.input);
    merge(target.render, source.render);
}
Json ProfilesJson(const FirstPlayableWorkflowResult& result)
{
    Json profiles = Json::array();
    for (const auto& profile : result.runtime.profiles)
    {
        Json diagnostics = Json::array();
        for (const auto& diagnostic : profile.diagnostics)
            diagnostics.push_back(DiagnosticJson(diagnostic));
        profiles.push_back({{"id", ToString(profile.profile)},
            {"status", ToString(profile.status)}, {"reportPath", profile.reportPath.string()},
            {"stdoutPath", profile.stdoutPath.string()}, {"stderrPath", profile.stderrPath.string()},
            {"started", profile.process.started}, {"timedOut", profile.process.timedOut},
            {"exitCode", profile.process.exitCode},
            {"durationMs", profile.process.duration.count()}, {"diagnostics", diagnostics}});
    }
    Json descriptorDiagnostics = Json::array();
    for (const auto& diagnostic : result.visualBlocksDescriptor.diagnostics)
        descriptorDiagnostics.push_back(DiagnosticJson(diagnostic));
    profiles.push_back({{"id", kVisualBlocksDescriptorProfileId},
        {"status", ToString(result.visualBlocksDescriptor.status)},
        {"generation", "InProcessReadOnlyGeneration"}, {"processLaunched", false},
        {"csharpExecuted", false},
        {"reportPath", result.visualBlocksDescriptor.reportPath.string()},
        {"diagnostics", descriptorDiagnostics}});
    return profiles;
}
Json GateJson(const FirstPlayableWorkflowResult& result,
              const RuntimeEvidenceCapabilities& capabilities)
{
    Json diagnostics = Json::array();
    for (const auto& diagnostic : result.gate.diagnostics)
        diagnostics.push_back(DiagnosticJson(diagnostic));
    Json checks = Json::array();
    for (const auto& check : result.gate.checks)
        checks.push_back({{"id", check.id}, {"status", ToString(check.status)}});
    Json manual = {{"status", ToString(result.gate.manualEvidence.status)},
        {"reportPath", result.gate.manualEvidence.reportPath ?
            Json(result.gate.manualEvidence.reportPath->string()) : Json(nullptr)},
        {"sha256", result.gate.manualEvidence.reportSha256},
        {"realDeviceObserved", result.gate.manualEvidence.realDeviceObserved},
        {"framesWithInput", result.gate.manualEvidence.framesWithInput}};
    return {{"schemaVersion", 1}, {"tool", "Saga"},
        {"command", "first-playable-check"},
        {"gate", {{"status", ToString(result.gate.status)},
            {"mandatoryProfiles", ToString(result.gate.mandatoryProfiles)},
            {"manualEvidence", {{"realKeyboard", manual}}},
            {"workspacePolicy", ToString(result.gate.workspacePolicy.status)},
            {"sourceIntegrity", ToString(result.gate.workspacePolicy.sourceIntegrity)},
            {"publicClaims", ToString(result.gate.publicClaims.status)},
            {"generatedEvidenceBundle", ToString(result.gate.evidenceBundle)}}},
        {"gateChecks", checks},
        {"workspacePolicy", {{"status", ToString(result.gate.workspacePolicy.status)},
            {"sourceRoot", result.gate.workspacePolicy.projectRoot.string()},
            {"outputRoot", result.gate.workspacePolicy.outputRoot.string()},
            {"sampleSourceModified", result.gate.workspacePolicy.sampleSourceModified},
            {"generatedFilesInsideSample", result.gate.workspacePolicy.generatedFilesInsideSample},
            {"sagaPrivatePolicy", "Passed"},
            {"sagaPrivateAccessedByWorkflow", false},
            {"attestationBasis", "NoWorkflowPathTargetsPrivateBoundary"}}},
        {"capabilities", {{"projectLoad", ToString(capabilities.project)},
            {"sceneLoad", ToString(capabilities.scene)},
            {"runtimeSmoke", ToString(capabilities.runtimeSmoke)},
            {"scriptMetadata", ToString(capabilities.scriptMetadata)},
            {"invocation", ToString(capabilities.invocation)},
            {"lifecycle", ToString(capabilities.lifecycle)},
            {"gameplayMutation", ToString(capabilities.gameplay)},
            {"inputSynthetic", ToString(capabilities.input)},
            {"inputKeyboardProvider", ToString(EvidenceStatus::ExternalTestEvidence)},
            {"inputKeyboardProviderEvidence", "StarterArenaPlayableTests"},
            {"inputRealKeyboard", ToString(result.gate.manualEvidence.status)},
            {"renderVisible", ToString(capabilities.render)},
            {"visualBlocksDescriptor", ToString(result.visualBlocksDescriptor.status)},
            {"workspacePolicy", ToString(result.gate.workspacePolicy.status)},
            {"productShellDiagnostics", diagnostics.empty() ? "Passed" : "Failed"}}},
        {"nonClaims", result.gate.nonClaims}, {"diagnostics", diagnostics}};
}
Json SummaryJson(const FirstPlayableWorkflowResult& result,
                 const RuntimeEvidenceCapabilities& capabilities)
{
    Json diagnostics = Json::array();
    for (const auto& diagnostic : result.gate.diagnostics)
        diagnostics.push_back(DiagnosticJson(diagnostic));
    Json checks = Json::array();
    for (const auto& check : result.gate.checks)
        checks.push_back({{"id", check.id}, {"status", ToString(check.status)}});
    return {{"schemaVersion", 1}, {"tool", "Saga"},
        {"command", "first-playable-check"}, {"status", ToString(result.status)},
        {"gateStatus", ToString(result.gate.status)},
        {"projectPath", result.runtime.projectManifest.string()},
        {"generatedReportDirectory", result.runtime.outputDirectory.string()},
        {"capabilities", {{"project", ToString(capabilities.project)},
            {"scene", ToString(capabilities.scene)},
            {"runtimeSmoke", ToString(capabilities.runtimeSmoke)},
            {"scriptMetadata", ToString(capabilities.scriptMetadata)},
            {"invocation", ToString(capabilities.invocation)},
            {"lifecycle", ToString(capabilities.lifecycle)},
            {"gameplay", ToString(capabilities.gameplay)},
            {"input", ToString(capabilities.input)}, {"render", ToString(capabilities.render)},
            {"visualBlocksDescriptor", ToString(result.visualBlocksDescriptor.status)}}},
        {"profiles", ProfilesJson(result)}, {"gateChecks", checks},
        {"manualEvidence", {{"realKeyboard", ToString(result.gate.manualEvidence.status)}}},
        {"gateReportPath", result.gatePath.string()},
        {"sourceManifestPath", result.sourceManifestPath.string()},
        {"evidenceManifestPath", result.evidenceManifestPath.string()},
        {"diagnostics", diagnostics}, {"nonClaims", result.gate.nonClaims}};
}
} // namespace

FirstPlayableWorkflowResult RunFirstPlayableWorkflow(
    const FirstPlayableWorkflowRequest& request, std::ostream& out, std::ostream& err)
{
    FirstPlayableWorkflowResult result;
    const auto outputRoot = std::filesystem::absolute(request.runtime.outputDirectory);
    result.summaryPath = std::filesystem::absolute(request.summaryPath.empty() ? outputRoot /
        "first_playable_summary.json" : request.summaryPath);
    result.gatePath = outputRoot / "first_playable_gate.json";
    result.sourceManifestPath = outputRoot / "source_manifest.json";
    result.evidenceManifestPath = outputRoot / "evidence_manifest.json";
    const auto session = FirstPlayableWorkspacePolicy::Begin(
        request.runtime.projectManifest, outputRoot, result.summaryPath);
    if (session.status == EvidenceStatus::Passed)
    {
        RuntimeEvidenceRunner runner;
        result.runtime = runner.Run(request.runtime);
        VisualBlocksDescriptorGenerationRequest descriptor;
        descriptor.originalProjectManifest = std::filesystem::absolute(request.runtime.projectManifest);
        descriptor.generatedWorkspace = result.runtime.outputDirectory / "workspace";
        descriptor.bindingManifestPath = result.runtime.scriptManifest;
        descriptor.artifactManifestPath = result.runtime.scriptArtifacts;
        descriptor.reportPath = result.runtime.outputDirectory / "profiles" /
            kVisualBlocksDescriptorProfileId / "report.json";
        for (const auto& profile : result.runtime.profiles)
        {
            if (profile.profile == RuntimeEvidenceProfile::StarterArenaLifecycle)
                descriptor.lifecycleEvidence = {profile.status, profile.reportPath};
            if (profile.profile == RuntimeEvidenceProfile::StarterArenaGameplay)
                descriptor.gameplayEvidence = {profile.status, profile.reportPath};
        }
        result.visualBlocksDescriptor = GenerateVisualBlocksDescriptorReport(descriptor);
    }
    else
    {
        result.runtime.projectManifest = std::filesystem::absolute(request.runtime.projectManifest);
        result.runtime.outputDirectory = std::filesystem::absolute(request.runtime.outputDirectory);
    }
    auto workspace = FirstPlayableWorkspacePolicy::Complete(session);
    auto manual = FirstPlayableManualEvidence::Validate(request.keyboardReportPath);
    auto claims = FirstPlayablePublicClaimAudit::Run(session.projectRoot);
    result.gate = FirstPlayableGate::Evaluate(result.runtime,
        result.visualBlocksDescriptor, std::move(workspace), std::move(manual), std::move(claims));
    result.gate.reportPath = result.gatePath;
    result.status = (result.gate.status == FirstPlayableGateStatus::Accepted ||
        result.gate.status == FirstPlayableGateStatus::AcceptedWithManualEvidencePending) ?
        EvidenceStatus::Passed : (result.gate.status == FirstPlayableGateStatus::Incomplete ?
        EvidenceStatus::Incomplete : EvidenceStatus::Failed);

    RuntimeEvidenceCapabilities capabilities;
    for (const auto& profile : result.runtime.profiles) Merge(capabilities, profile.report.capabilities);
    if (session.status == EvidenceStatus::Passed)
    {
        const bool gateWritten = FirstPlayableEvidenceBundle::WriteJson(
            result.gatePath, GateJson(result, capabilities),
            result.gate.diagnostics, "ProductShell.FirstPlayable.GateReportWriteFailed");
        const bool summaryWritten = FirstPlayableEvidenceBundle::WriteJson(
            result.summaryPath, SummaryJson(result, capabilities),
            result.gate.diagnostics, "ProductShell.FirstPlayable.GateReportWriteFailed");
        if (!gateWritten || !summaryWritten)
        {
            result.gate.status = FirstPlayableGateStatus::Rejected;
            result.status = EvidenceStatus::Failed;
        }
        auto bundle = FirstPlayableEvidenceBundle::Write(result.runtime.outputDirectory,
            result.runtime, result.visualBlocksDescriptor, result.gate, result.summaryPath, result.gatePath);
        result.sourceManifestPath = bundle.sourceManifestPath;
        result.evidenceManifestPath = bundle.evidenceManifestPath;
        if (bundle.status != EvidenceStatus::Passed)
        {
            result.gate.evidenceBundle = EvidenceStatus::Failed;
            result.gate.status = FirstPlayableGateStatus::Rejected;
            result.status = EvidenceStatus::Failed;
            result.gate.diagnostics.insert(result.gate.diagnostics.end(),
                bundle.diagnostics.begin(), bundle.diagnostics.end());
            for (auto& check : result.gate.checks)
                if (check.id == "evidence-bundle") check.status = EvidenceStatus::Failed;
            const bool rewrittenGate = FirstPlayableEvidenceBundle::WriteJson(
                result.gatePath, GateJson(result, capabilities),
                result.gate.diagnostics, "ProductShell.FirstPlayable.GateReportWriteFailed");
            const bool rewrittenSummary = FirstPlayableEvidenceBundle::WriteJson(
                result.summaryPath, SummaryJson(result, capabilities),
                result.gate.diagnostics, "ProductShell.FirstPlayable.GateReportWriteFailed");
            if (!rewrittenGate || !rewrittenSummary)
                err << "ProductShell.FirstPlayable.GateReportWriteFailed: could not finalize rejected gate\n";
        }
    }
    out << "Saga First Playable Check\n\nOverall Gate: " << ToString(result.gate.status)
        << "\n\nMandatory Evidence:\n  Project Load: " << ToString(capabilities.project)
        << "\n  Scene Load: " << ToString(capabilities.scene)
        << "\n  Runtime Smoke: " << ToString(capabilities.runtimeSmoke)
        << "\n  Script Metadata: " << ToString(capabilities.scriptMetadata)
        << "\n  C# Invocation: " << ToString(capabilities.invocation)
        << "\n  C# Lifecycle: " << ToString(capabilities.lifecycle)
        << "\n  C# Gameplay Mutation: " << ToString(capabilities.gameplay)
        << "\n  Synthetic Input: " << ToString(capabilities.input)
        << "\n  Visible Render: " << ToString(capabilities.render)
        << "\n  Visual Blocks Descriptor: " << ToString(result.visualBlocksDescriptor.status)
        << "\n\nManual Evidence:\n  Real Keyboard: " << ToString(result.gate.manualEvidence.status)
        << "\n  Keyboard Provider: ExternalTestEvidence (StarterArenaPlayableTests)"
        << "\n\nPolicy:\n  Workspace Policy: " << ToString(result.gate.workspacePolicy.status)
        << "\n  Source Integrity: " << ToString(result.gate.workspacePolicy.sourceIntegrity)
        << "\n  Public Claims: " << ToString(result.gate.publicClaims.status)
        << "\n  Evidence Bundle: " << ToString(result.gate.evidenceBundle)
        << "\n\nReports:\n  Summary: " << result.summaryPath
        << "\n  Gate: " << result.gatePath << "\n  Manifest: " << result.evidenceManifestPath
        << "\nDiagnostics: " << result.gate.diagnostics.size() << '\n';
    for (const auto& diagnostic : result.gate.diagnostics)
        err << diagnostic.code << ": " << diagnostic.message << '\n';
    return result;
}
} // namespace SagaProduct
