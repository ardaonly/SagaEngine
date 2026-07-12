/// @file RuntimeEvidenceReport.cpp
/// @brief Product-owned runtime evidence parsing and profile validation.

#include "RuntimeEvidenceReport.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>
#include <utility>

namespace SagaProduct
{
namespace
{

using Json = nlohmann::json;

[[nodiscard]] EvidenceStatus ParseStatus(const Json& object,
                                         const char* key,
                                         EvidenceStatus fallback)
{
    if (!object.is_object() || !object.contains(key) ||
        !object[key].is_string())
    {
        return fallback;
    }
    const std::string value = object[key].get<std::string>();
    if (value == "Passed") return EvidenceStatus::Passed;
    if (value == "Failed") return EvidenceStatus::Failed;
    if (value == "NotRequested") return EvidenceStatus::NotRequested;
    if (value == "NotPresent") return EvidenceStatus::NotPresent;
    if (value == "NoInputObserved") return EvidenceStatus::NoInputObserved;
    if (value == "NotInitialized") return EvidenceStatus::NotPresent;
    return EvidenceStatus::Incomplete;
}

void AddError(RuntimeEvidenceReport& report,
              const std::string& profileId,
              const std::string& code,
              std::string message,
              const std::filesystem::path& path = {})
{
    FirstPlayableDiagnostic diagnostic;
    diagnostic.severity = EvidenceSeverity::Error;
    diagnostic.code = code;
    diagnostic.message = std::move(message);
    diagnostic.profileId = profileId;
    if (!path.empty()) diagnostic.path = path;
    report.diagnostics.push_back(std::move(diagnostic));
    report.status = EvidenceStatus::Failed;
}

[[nodiscard]] const Json& Section(const Json& root, const char* key)
{
    static const Json empty = Json::object();
    return root.contains(key) && root[key].is_object() ? root[key] : empty;
}

[[nodiscard]] bool HasPositiveInteger(const Json& object, const char* key)
{
    return object.contains(key) && object[key].is_number_integer() &&
        object[key].get<long long>() > 0;
}

[[nodiscard]] bool HasString(const Json& object, const char* key,
                             const std::string& expected)
{
    return object.contains(key) && object[key].is_string() &&
        object[key].get<std::string>() == expected;
}

[[nodiscard]] bool ContainsString(const Json& array, const std::string& value)
{
    if (!array.is_array()) return false;
    for (const Json& item : array)
        if (item.is_string() && item.get<std::string>() == value) return true;
    return false;
}

void RequirePassed(RuntimeEvidenceReport& report,
                   const std::string& profileId,
                   EvidenceStatus actual,
                   const std::string& field)
{
    if (actual != EvidenceStatus::Passed)
    {
        AddError(report, profileId,
                 "ProductShell.FirstPlayable.ReportStatusMismatch",
                 field + " must be Passed");
    }
}

void ValidateGameplay(RuntimeEvidenceReport& report,
                      const Json& root,
                      const std::string& profileId)
{
    const Json& gameplay = Section(root, "gameplay");
    report.capabilities.gameplay = ParseStatus(gameplay, "status",
                                                EvidenceStatus::NotPresent);
    const Json& gameplayLifecycle = Section(gameplay, "lifecycle");
    if (report.capabilities.lifecycle == EvidenceStatus::NotRequested &&
        gameplayLifecycle.contains("callbacksObserved") &&
        gameplayLifecycle["callbacksObserved"].is_array() &&
        gameplayLifecycle["callbacksObserved"].size() == 4)
        report.capabilities.lifecycle = EvidenceStatus::Passed;
    RequirePassed(report, profileId, report.capabilities.gameplay,
                  "gameplay.status");
    const Json& finalState = Section(gameplay, "finalState");
    if (!finalState.contains("score") || !finalState["score"].is_number_integer() ||
        finalState["score"].get<int>() != 10 ||
        !finalState.value("pickupCollected", false) ||
        !HasString(finalState, "playerState", "powered"))
    {
        AddError(report, profileId,
                 "ProductShell.FirstPlayable.ReportStatusMismatch",
                 "gameplay final state must be score 10, collected, and powered");
    }
    const Json& pickup = Section(gameplay, "pickup");
    const Json& pickupPosition = Section(pickup, "position");
    const Json& trigger = Section(gameplay, "trigger");
    const Json& playerPosition = Section(trigger, "playerPosition");
    if (!pickupPosition.contains("x") || !pickupPosition.contains("y") ||
        !playerPosition.contains("x") || !playerPosition.contains("y") ||
        !pickup.value("radius", 0.0))
    {
        AddError(report, profileId,
                 "ProductShell.FirstPlayable.ReportStatusMismatch",
                 "gameplay trigger position and pickup radius are required");
    }
    else
    {
        const double dx = playerPosition.value("x", 0.0) -
            pickupPosition.value("x", 0.0);
        const double dy = playerPosition.value("y", 0.0) -
            pickupPosition.value("y", 0.0);
        const double radius = pickup.value("radius", 0.0);
        if (std::sqrt(dx * dx + dy * dy) > radius + 1e-9)
        {
            AddError(report, profileId,
                     "ProductShell.FirstPlayable.ReportStatusMismatch",
                     "gameplay trigger.playerPosition is outside pickup radius");
        }
    }
    if (!gameplay.contains("mutations") || !gameplay["mutations"].is_array() ||
        gameplay["mutations"].size() < 3)
    {
        AddError(report, profileId,
                 "ProductShell.FirstPlayable.ReportStatusMismatch",
                 "gameplay mutation evidence is incomplete");
    }
}

} // namespace

const char* ToString(EvidenceStatus status) noexcept
{
    switch (status)
    {
        case EvidenceStatus::Passed: return "Passed";
        case EvidenceStatus::Warning: return "Warning";
        case EvidenceStatus::Failed: return "Failed";
        case EvidenceStatus::Incomplete: return "Incomplete";
        case EvidenceStatus::NotRequested: return "NotRequested";
        case EvidenceStatus::NotPresent: return "NotPresent";
        case EvidenceStatus::NoInputObserved: return "NoInputObserved";
        case EvidenceStatus::PendingManualEvidence: return "PendingManualEvidence";
        case EvidenceStatus::ExternalTestEvidence: return "ExternalTestEvidence";
    }
    return "Incomplete";
}

const char* ToString(EvidenceSeverity severity) noexcept
{
    switch (severity)
    {
        case EvidenceSeverity::Info: return "Info";
        case EvidenceSeverity::Warning: return "Warning";
        case EvidenceSeverity::Error: return "Error";
    }
    return "Error";
}

RuntimeEvidenceReport ParseRuntimeEvidenceReport(
    const std::filesystem::path& reportPath,
    const std::string& profileId)
{
    RuntimeEvidenceReport report;
    std::ifstream input(reportPath);
    if (!input)
    {
        AddError(report, profileId,
                 "ProductShell.FirstPlayable.ReportMissing",
                 "runtime report is missing", reportPath);
        return report;
    }

    Json root;
    try { input >> root; }
    catch (const std::exception& error)
    {
        AddError(report, profileId,
                 "ProductShell.FirstPlayable.ReportMalformed",
                 std::string("runtime report is not valid JSON: ") + error.what(),
                 reportPath);
        return report;
    }
    if (!root.is_object() || !root.contains("status") ||
        !root["status"].is_string())
    {
        AddError(report, profileId,
                 "ProductShell.FirstPlayable.ReportMalformed",
                 "runtime report requires a string top-level status", reportPath);
        return report;
    }

    report.status = ParseStatus(root, "status", EvidenceStatus::Incomplete);
    if (report.status != EvidenceStatus::Passed)
    {
        AddError(report, profileId,
                 "ProductShell.FirstPlayable.ReportStatusMismatch",
                 "runtime report top-level status must be Passed", reportPath);
    }
    const Json& project = Section(root, "project");
    report.projectId = project.value("projectId", "");
    report.capabilities.project = report.projectId.empty() ?
        EvidenceStatus::NotPresent : EvidenceStatus::Passed;
    if (report.projectId != "starter-arena")
    {
        AddError(report, profileId,
                 "ProductShell.FirstPlayable.UnsupportedProject",
                 "report project.projectId must be starter-arena", reportPath);
    }
    const Json& scene = Section(root, "scene");
    report.sceneId = scene.value("sceneId", "");
    report.capabilities.scene = report.sceneId.empty() ?
        EvidenceStatus::NotPresent : EvidenceStatus::Passed;
    report.capabilities.runtimeSmoke = EvidenceStatus::Passed;

    const Json& binding = Section(root, "scriptBinding");
    report.capabilities.scriptMetadata = ParseStatus(
        binding, "status", EvidenceStatus::NotRequested);
    report.capabilities.invocation = ParseStatus(
        Section(root, "scriptInvocation"), "status", EvidenceStatus::NotRequested);
    report.capabilities.lifecycle = ParseStatus(
        Section(root, "scriptLifecycle"), "status", EvidenceStatus::NotRequested);
    report.capabilities.gameplay = ParseStatus(
        Section(root, "gameplay"), "status", EvidenceStatus::NotRequested);
    report.capabilities.input = ParseStatus(
        Section(root, "input"), "status", EvidenceStatus::NotPresent);
    report.capabilities.render = ParseStatus(
        Section(root, "render"), "status", EvidenceStatus::NotPresent);

    if (root.contains("nonClaims") && root["nonClaims"].is_array())
    {
        for (const Json& value : root["nonClaims"])
            if (value.is_string()) report.nonClaims.push_back(value.get<std::string>());
    }
    if (root.contains("diagnostics"))
    {
        if (!root["diagnostics"].is_array())
        {
            AddError(report, profileId,
                     "ProductShell.FirstPlayable.ReportMalformed",
                     "diagnostics must be an array", reportPath);
        }
        else if (!root["diagnostics"].empty())
        {
            AddError(report, profileId,
                     "ProductShell.FirstPlayable.DiagnosticsPresent",
                     "passed runtime report contains diagnostics", reportPath);
        }
    }

    if (profileId == "starter-arena-smoke")
    {
        const Json& loop = Section(root, "loop");
        RequirePassed(report, profileId, ParseStatus(loop, "status",
            EvidenceStatus::NotPresent), "loop.status");
        if (!HasPositiveInteger(loop, "frames"))
            AddError(report, profileId,
                "ProductShell.FirstPlayable.ReportStatusMismatch",
                "loop.frames must be positive");
    }
    else if (profileId == "starter-arena-lifecycle")
    {
        RequirePassed(report, profileId, report.capabilities.scriptMetadata,
                      "scriptBinding.status");
        RequirePassed(report, profileId, report.capabilities.invocation,
                      "scriptInvocation.status");
        RequirePassed(report, profileId, report.capabilities.lifecycle,
                      "scriptLifecycle.status");
        const Json& invocation = Section(root, "scriptInvocation");
        if (invocation.value("result", 0) != 15)
            AddError(report, profileId,
                "ProductShell.FirstPlayable.ReportStatusMismatch",
                "scriptInvocation.result must be 15");
        const Json& lifecycle = Section(root, "scriptLifecycle");
        if (!lifecycle.contains("callbacksObserved") ||
            !ContainsString(lifecycle["callbacksObserved"], "OnCreate") ||
            !ContainsString(lifecycle["callbacksObserved"], "OnStart") ||
            !ContainsString(lifecycle["callbacksObserved"], "OnUpdate") ||
            !ContainsString(lifecycle["callbacksObserved"], "OnDestroy"))
            AddError(report, profileId,
                "ProductShell.FirstPlayable.ReportStatusMismatch",
                "script lifecycle must report four callbacks");
    }
    else if (profileId == "starter-arena-gameplay" ||
             profileId == "starter-arena-visible-gameplay")
    {
        ValidateGameplay(report, root, profileId);
        RequirePassed(report, profileId, report.capabilities.lifecycle,
                      "script lifecycle evidence");
    }

    if (profileId == "starter-arena-visible-synthetic" ||
        profileId == "starter-arena-visible-gameplay")
    {
        const Json& inputSection = Section(root, "input");
        const Json& render = Section(root, "render");
        RequirePassed(report, profileId, report.capabilities.input, "input.status");
        RequirePassed(report, profileId, report.capabilities.render, "render.status");
        if (!HasString(inputSection, "source", "synthetic"))
            AddError(report, profileId,
                "ProductShell.FirstPlayable.ReportStatusMismatch",
                "input.source must be synthetic");
        if (!HasPositiveInteger(render, "requestedFrames") ||
            !HasPositiveInteger(render, "presentedFrames"))
            AddError(report, profileId,
                "ProductShell.FirstPlayable.ReportStatusMismatch",
                "render frame counts must be positive");
        const Json& finalPosition = Section(Section(root, "simulation"),
                                            "finalPosition");
        if (std::abs(finalPosition.value("x", -99.0) - 0.48) > 1e-6 ||
            std::abs(finalPosition.value("y", -99.0) - 0.48) > 1e-6)
            AddError(report, profileId,
                "ProductShell.FirstPlayable.ReportStatusMismatch",
                "simulation.finalPosition must be approximately (0.48, 0.48)");
        if (profileId == "starter-arena-visible-gameplay" &&
            (!render.value("gameplayStateReflected", false) ||
             render.value("pickupDrawSubmittedLastFrame", true) ||
             !render.value("poweredPlayerDrawSubmitted", false)))
            AddError(report, profileId,
                "ProductShell.FirstPlayable.ReportStatusMismatch",
                "visible gameplay reflection contract was not satisfied");
    }

    if (report.diagnostics.empty()) report.status = EvidenceStatus::Passed;
    return report;
}

} // namespace SagaProduct
