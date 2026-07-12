/// @file FirstPlayableWorkflow.cpp
/// @brief Consolidated Product Shell first-playable diagnostics workflow.

#include "FirstPlayableWorkflow.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <ostream>

namespace SagaProduct
{
namespace
{

using Json = nlohmann::json;

[[nodiscard]] Json DiagnosticJson(const FirstPlayableDiagnostic& diagnostic)
{
    Json value = {
        {"severity", ToString(diagnostic.severity)},
        {"code", diagnostic.code},
        {"message", diagnostic.message},
    };
    if (diagnostic.profileId) value["profileId"] = *diagnostic.profileId;
    if (diagnostic.path) value["path"] = diagnostic.path->string();
    if (diagnostic.actionHint) value["actionHint"] = *diagnostic.actionHint;
    return value;
}

void MergeCapabilities(RuntimeEvidenceCapabilities& target,
                       const RuntimeEvidenceCapabilities& source)
{
    auto merge = [](EvidenceStatus& current, EvidenceStatus incoming)
    {
        if (incoming == EvidenceStatus::Passed ||
            (current == EvidenceStatus::NotPresent &&
             incoming != EvidenceStatus::NotPresent) ||
            (current == EvidenceStatus::NotRequested &&
             incoming != EvidenceStatus::NotRequested))
            current = incoming;
    };
    merge(target.project, source.project);
    merge(target.scene, source.scene);
    merge(target.runtimeSmoke, source.runtimeSmoke);
    merge(target.scriptMetadata, source.scriptMetadata);
    merge(target.invocation, source.invocation);
    merge(target.lifecycle, source.lifecycle);
    merge(target.gameplay, source.gameplay);
    merge(target.input, source.input);
    merge(target.render, source.render);
}

} // namespace

FirstPlayableWorkflowResult RunFirstPlayableWorkflow(
    const FirstPlayableWorkflowRequest& request,
    std::ostream& out,
    std::ostream& err)
{
    FirstPlayableWorkflowResult result;
    result.summaryPath = request.summaryPath.empty() ?
        request.runtime.outputDirectory / "first_playable_summary.json" :
        request.summaryPath;
    RuntimeEvidenceRunner runner;
    result.runtime = runner.Run(request.runtime);
    result.status = result.runtime.prepared && result.runtime.profiles.size() == 5 ?
        EvidenceStatus::Passed : EvidenceStatus::Failed;

    RuntimeEvidenceCapabilities capabilities;
    Json profiles = Json::array();
    Json diagnostics = Json::array();
    for (const FirstPlayableDiagnostic& diagnostic : result.runtime.diagnostics)
        diagnostics.push_back(DiagnosticJson(diagnostic));
    for (const RuntimeEvidenceProfileResult& profile : result.runtime.profiles)
    {
        MergeCapabilities(capabilities, profile.report.capabilities);
        Json profileDiagnostics = Json::array();
        for (const FirstPlayableDiagnostic& diagnostic : profile.diagnostics)
        {
            profileDiagnostics.push_back(DiagnosticJson(diagnostic));
            diagnostics.push_back(DiagnosticJson(diagnostic));
        }
        profiles.push_back({
            {"id", ToString(profile.profile)},
            {"status", ToString(profile.status)},
            {"reportPath", profile.reportPath.string()},
            {"stdoutPath", profile.stdoutPath.string()},
            {"stderrPath", profile.stderrPath.string()},
            {"started", profile.process.started},
            {"timedOut", profile.process.timedOut},
            {"exitCode", profile.process.exitCode},
            {"durationMs", profile.process.duration.count()},
            {"diagnostics", profileDiagnostics},
        });
        if (profile.status != EvidenceStatus::Passed)
            result.status = EvidenceStatus::Failed;
    }

    const Json capabilityJson = {
        {"project", ToString(capabilities.project)},
        {"scene", ToString(capabilities.scene)},
        {"runtimeSmoke", ToString(capabilities.runtimeSmoke)},
        {"scriptMetadata", ToString(capabilities.scriptMetadata)},
        {"invocation", ToString(capabilities.invocation)},
        {"lifecycle", ToString(capabilities.lifecycle)},
        {"gameplay", ToString(capabilities.gameplay)},
        {"input", ToString(capabilities.input)},
        {"render", ToString(capabilities.render)},
    };
    const Json summary = {
        {"schemaVersion", 1},
        {"tool", "Saga"},
        {"command", "first-playable-check"},
        {"status", ToString(result.status)},
        {"projectPath", result.runtime.projectManifest.string()},
        {"generatedReportDirectory", result.runtime.outputDirectory.string()},
        {"capabilities", capabilityJson},
        {"profiles", profiles},
        {"manualEvidence", {{"realKeyboard", "PendingManualEvidence"}}},
        {"diagnostics", diagnostics},
        {"nonClaims", {
            "No full editor workflow claim",
            "No Visual Blocks canvas claim",
            "No networking or multiplayer claim",
            "No package install or distribution claim",
            "No production readiness claim",
        }},
    };

    std::error_code ec;
    std::filesystem::create_directories(result.summaryPath.parent_path(), ec);
    std::ofstream summaryOutput(result.summaryPath, std::ios::trunc);
    if (ec || !summaryOutput)
    {
        result.status = EvidenceStatus::Failed;
        err << "ProductShell.FirstPlayable.ReportDirectoryInvalid: cannot write "
            << result.summaryPath << '\n';
    }
    else
    {
        summaryOutput << summary.dump(2) << '\n';
    }

    out << "StarterArena First Playable Evidence\n"
        << "Project: " << ToString(capabilities.project) << '\n'
        << "Scene: " << ToString(capabilities.scene) << '\n'
        << "Runtime Smoke: " << ToString(capabilities.runtimeSmoke) << '\n'
        << "Script Metadata: " << ToString(capabilities.scriptMetadata) << '\n'
        << "Invocation: " << ToString(capabilities.invocation) << '\n'
        << "Lifecycle: " << ToString(capabilities.lifecycle) << '\n'
        << "Gameplay: " << ToString(capabilities.gameplay) << '\n'
        << "Input: " << ToString(capabilities.input) << '\n'
        << "Render: " << ToString(capabilities.render) << '\n'
        << "Real Keyboard: PendingManualEvidence\n"
        << "Diagnostics: " << diagnostics.size() << '\n'
        << "Generated Reports: " << result.runtime.outputDirectory << '\n'
        << "Summary: " << result.summaryPath << '\n'
        << "Overall: " << ToString(result.status) << '\n';
    return result;
}

} // namespace SagaProduct
