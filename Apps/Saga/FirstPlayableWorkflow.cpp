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

const char* ToString(FirstPlayableOperatorMode mode) noexcept
{
    return mode == FirstPlayableOperatorMode::Capture ? "Capture" : "Import";
}

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
    const bool keyboardPackaged =
        result.gate.manualEvidence.status == EvidenceStatus::Passed &&
        std::filesystem::is_regular_file(result.runtime.outputDirectory /
            "manual" / "real_keyboard_report.json");
    const bool capturePackaged = std::filesystem::is_regular_file(
        result.runtime.outputDirectory / "manual" /
        "real_keyboard_capture_report.json");
    Json manual = {{"status", ToString(result.gate.manualEvidence.status)},
        {"reportPath", keyboardPackaged ?
            Json("manual/real_keyboard_report.json") : Json(nullptr)},
        {"originalReportPath", result.gate.manualEvidence.reportPath ?
            Json(result.gate.manualEvidence.reportPath->string()) : Json(nullptr)},
        {"captureReportPath", capturePackaged ?
            Json("manual/real_keyboard_capture_report.json") : Json(nullptr)},
        {"sha256", result.gate.manualEvidence.reportSha256},
        {"realDeviceObserved", result.gate.manualEvidence.realDeviceObserved},
        {"framesWithInput", result.gate.manualEvidence.framesWithInput},
        {"movementDistance", result.gate.manualEvidence.movementDistance}};
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

void OperatorFailure(FirstPlayableWorkflowResult& result,
                     const std::filesystem::path& path,
                     std::string message)
{
    FirstPlayableDiagnostic diagnostic;
    diagnostic.code = "ProductShell.HumanCapture.ReportImportFailed";
    diagnostic.message = std::move(message);
    diagnostic.path = path;
    diagnostic.actionHint = "Use a fresh external output root and retry the human capture.";
    result.gate.diagnostics.push_back(std::move(diagnostic));
    result.gate.status = FirstPlayableGateStatus::Rejected;
    result.status = EvidenceStatus::Failed;
    for (auto& check : result.gate.checks)
        if (check.id == "human-capture") check.status = EvidenceStatus::Failed;
}

bool CopyOperatorArtifact(const std::filesystem::path& source,
                          const std::filesystem::path& destination,
                          bool required,
                          FirstPlayableWorkflowResult& result)
{
    if (source.empty() || !std::filesystem::is_regular_file(source))
    {
        if (required)
            OperatorFailure(result, source,
                "required human-capture artifact is missing");
        return !required;
    }
    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (!ec) std::filesystem::copy_file(source, destination,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
    {
        OperatorFailure(result, destination,
            "could not import human-capture artifact: " + ec.message());
        return false;
    }
    return true;
}

bool ImportValidatedKeyboardEvidence(FirstPlayableWorkflowResult& result)
{
    if (result.gate.manualEvidence.status != EvidenceStatus::Passed ||
        !result.gate.manualEvidence.reportPath)
        return true;
    const auto imported = result.runtime.outputDirectory / "manual" /
        "real_keyboard_report.json";
    std::error_code ec;
    std::filesystem::create_directories(imported.parent_path(), ec);
    if (!ec) std::filesystem::copy_file(*result.gate.manualEvidence.reportPath,
        imported, std::filesystem::copy_options::overwrite_existing, ec);
    if (!ec) return true;
    FirstPlayableDiagnostic diagnostic;
    diagnostic.code = "ProductShell.FirstPlayable.EvidenceManifestWriteFailed";
    diagnostic.message = "could not stage validated keyboard evidence: " + ec.message();
    diagnostic.path = imported;
    diagnostic.actionHint = "Use a writable fresh evidence output directory and retry.";
    result.gate.diagnostics.push_back(std::move(diagnostic));
    result.gate.evidenceBundle = EvidenceStatus::Failed;
    result.gate.status = FirstPlayableGateStatus::Rejected;
    result.status = EvidenceStatus::Failed;
    for (auto& check : result.gate.checks)
        if (check.id == "evidence-bundle") check.status = EvidenceStatus::Failed;
    return false;
}

Json PositionJson(const std::optional<FirstPlayableEvidencePosition>& position)
{
    if (!position) return nullptr;
    return {{"x", position->x}, {"y", position->y}};
}

bool WriteOperatorEvidence(const FirstPlayableOperatorEvidenceInput& evidence,
                           FirstPlayableWorkflowResult& result)
{
    const auto manualRoot = result.runtime.outputDirectory / "manual";
    const bool required = evidence.status == EvidenceStatus::Passed;
    if (evidence.mode == FirstPlayableOperatorMode::Capture)
    {
        CopyOperatorArtifact(evidence.captureReportPath,
            manualRoot / "real_keyboard_capture_report.json", required, result);
        CopyOperatorArtifact(evidence.stdoutPath,
            manualRoot / "keyboard_capture_stdout.txt", required, result);
        CopyOperatorArtifact(evidence.stderrPath,
            manualRoot / "keyboard_capture_stderr.txt", required, result);
    }
    Json diagnostics = Json::array();
    for (const auto& diagnostic : evidence.diagnostics)
        diagnostics.push_back(DiagnosticJson(diagnostic));
    for (const auto& diagnostic : result.gate.manualEvidence.diagnostics)
        diagnostics.push_back(DiagnosticJson(diagnostic));
    EvidenceStatus operatorStatus = evidence.status;
    for (const auto& check : result.gate.checks)
        if (check.id == "human-capture") operatorStatus = check.status;
    Json runtime = {{"started", false}, {"timedOut", false}, {"exitCode", nullptr},
        {"durationMs", 0}, {"stdoutPath", nullptr}, {"stderrPath", nullptr},
        {"reportPath", nullptr}};
    if (evidence.process)
    {
        runtime = {{"started", evidence.process->started},
            {"timedOut", evidence.process->timedOut},
            {"exitCode", evidence.process->exitCode},
            {"durationMs", evidence.process->duration.count()},
            {"stdoutPath", "manual/keyboard_capture_stdout.txt"},
            {"stderrPath", "manual/keyboard_capture_stderr.txt"},
            {"reportPath", "manual/real_keyboard_capture_report.json"}};
    }
    const Json report = {{"schemaVersion", 1}, {"tool", "Saga"},
        {"command", "first-playable-human-capture"},
        {"status", ToString(operatorStatus)}, {"mode", ToString(evidence.mode)},
        {"runtime", std::move(runtime)},
        {"keyboardEvidence", {
            {"status", ToString(result.gate.manualEvidence.status)},
            {"source", result.gate.manualEvidence.source},
            {"realDeviceObserved", result.gate.manualEvidence.realDeviceObserved},
            {"framesWithInput", result.gate.manualEvidence.framesWithInput},
            {"initialPosition", PositionJson(result.gate.manualEvidence.initialPosition)},
            {"finalPosition", PositionJson(result.gate.manualEvidence.finalPosition)},
            {"movementDistance", result.gate.manualEvidence.movementDistance},
            {"sha256", result.gate.manualEvidence.reportSha256},
            {"originalReportPath", evidence.originalReportPath.string()},
            {"importedReportPath", result.gate.manualEvidence.status == EvidenceStatus::Passed ?
                Json("manual/real_keyboard_report.json") : Json(nullptr)}}},
        {"gateStatus", ToString(result.gate.status)},
        {"nonClaims", Json::array({"No broad interactive gameplay claim",
            "No production input UX claim", "No platform-wide device compatibility claim",
            "No full game or editor claim", "No package install or distribution claim"})},
        {"diagnostics", std::move(diagnostics)}};
    return FirstPlayableEvidenceBundle::WriteJson(
        manualRoot / "human_capture_report.json", report,
        result.gate.diagnostics, "ProductShell.HumanCapture.ReportImportFailed");
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
    const auto session = request.workspaceSession ? *request.workspaceSession :
        FirstPlayableWorkspacePolicy::Begin(
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
    std::optional<FirstPlayableGateCheck> operatorCheck;
    if (request.operatorEvidence)
        operatorCheck = FirstPlayableGateCheck{"human-capture",
            request.operatorEvidence->status};
    result.gate = FirstPlayableGate::Evaluate(result.runtime,
        result.visualBlocksDescriptor, std::move(workspace), std::move(manual),
        std::move(claims), operatorCheck);
    if (request.operatorEvidence)
        result.gate.diagnostics.insert(result.gate.diagnostics.end(),
            request.operatorEvidence->diagnostics.begin(),
            request.operatorEvidence->diagnostics.end());
    result.gate.reportPath = result.gatePath;
    result.status = (result.gate.status == FirstPlayableGateStatus::Accepted ||
        result.gate.status == FirstPlayableGateStatus::AcceptedWithManualEvidencePending) ?
        EvidenceStatus::Passed : (result.gate.status == FirstPlayableGateStatus::Incomplete ?
        EvidenceStatus::Incomplete : EvidenceStatus::Failed);

    RuntimeEvidenceCapabilities capabilities;
    for (const auto& profile : result.runtime.profiles) Merge(capabilities, profile.report.capabilities);
    if (session.status == EvidenceStatus::Passed)
    {
        if (request.operatorEvidence &&
            !WriteOperatorEvidence(*request.operatorEvidence, result))
            OperatorFailure(result, result.runtime.outputDirectory / "manual" /
                "human_capture_report.json", "could not write human-capture report");
        ImportValidatedKeyboardEvidence(result);
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
