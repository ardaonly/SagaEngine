// SPDX-License-Identifier: Apache-2.0

/// @file HeadlessEvaluation.cpp
/// @brief Implements bounded MultiplayerSandbox server-side evaluation proof.

#include "MultiplayerSandboxHeadless/HeadlessEvaluation.hpp"

#include "SagaEngine/ServerAuthority/Simulation/AuthoritativeMovementCore.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <optional>
#include <system_error>
#include <utility>

namespace MultiplayerSandboxHeadless
{
namespace
{

using Json = nlohmann::ordered_json;
using SagaEngine::Networking::ClientId;
using SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementCore;
using SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementDecision;
using SagaEngine::ServerAuthority::Simulation::EntityId;
using SagaEngine::ServerAuthority::Simulation::InputCommand;
using SagaEngine::ServerAuthority::Simulation::Vector3;

constexpr ClientId kClientId = 1;
constexpr EntityId kEntityId = 1001;

[[nodiscard]] HeadlessEvaluationDiagnostic MakeDiagnostic(
    std::string code,
    std::string message,
    std::filesystem::path path = {})
{
    HeadlessEvaluationDiagnostic diagnostic;
    diagnostic.code = std::move(code);
    diagnostic.message = std::move(message);
    diagnostic.path = std::move(path);
    return diagnostic;
}

[[nodiscard]] HeadlessEvaluationDiagnostic MakeInfoDiagnostic(
    std::string code,
    std::string message,
    std::filesystem::path path = {})
{
    HeadlessEvaluationDiagnostic diagnostic =
        MakeDiagnostic(std::move(code), std::move(message), std::move(path));
    diagnostic.severity = "Info";
    return diagnostic;
}

[[nodiscard]] std::filesystem::path ResolveManifestPath(
    const std::filesystem::path& projectPath,
    const char* defaultManifestName)
{
    if (projectPath.empty())
    {
        return {};
    }
    if (projectPath.extension() == ".sagaproj")
    {
        return projectPath;
    }
    return projectPath / defaultManifestName;
}

[[nodiscard]] std::optional<std::string> ReadProjectId(
    const std::filesystem::path& manifestPath,
    const char* sampleName,
    const char* diagnosticPrefix,
    std::vector<HeadlessEvaluationDiagnostic>& diagnostics)
{
    if (!std::filesystem::exists(manifestPath))
    {
        diagnostics.push_back(MakeDiagnostic(
            std::string(diagnosticPrefix) + ".Project.ManifestMissing",
            std::string(sampleName) + " .sagaproj manifest is missing",
            manifestPath));
        return std::nullopt;
    }

    std::ifstream input(manifestPath);
    if (!input.is_open())
    {
        diagnostics.push_back(MakeDiagnostic(
            std::string(diagnosticPrefix) + ".Project.ManifestOpenFailed",
            std::string(sampleName) + " .sagaproj manifest cannot be opened",
            manifestPath));
        return std::nullopt;
    }

    Json json;
    try
    {
        input >> json;
    }
    catch (const Json::exception& e)
    {
        diagnostics.push_back(MakeDiagnostic(
            std::string(diagnosticPrefix) + ".Project.ManifestParseFailed",
            std::string(sampleName) + " .sagaproj JSON is invalid: " + e.what(),
            manifestPath));
        return std::nullopt;
    }

    if (!json.contains("projectId") || !json["projectId"].is_string())
    {
        diagnostics.push_back(MakeDiagnostic(
            std::string(diagnosticPrefix) + ".Project.ProjectIdMissing",
            std::string(sampleName) + " .sagaproj must contain string projectId",
            manifestPath));
        return std::nullopt;
    }

    return json["projectId"].get<std::string>();
}

[[nodiscard]] const char* ToString(AuthoritativeMovementDecision decision) noexcept
{
    switch (decision)
    {
    case AuthoritativeMovementDecision::Queued:
        return "Queued";
    case AuthoritativeMovementDecision::RejectUnknownClient:
        return "RejectUnknownClient";
    case AuthoritativeMovementDecision::RejectOwnership:
        return "RejectOwnership";
    case AuthoritativeMovementDecision::RejectInput:
        return "RejectInput";
    case AuthoritativeMovementDecision::RejectQueue:
        return "RejectQueue";
    case AuthoritativeMovementDecision::Accepted:
        return "Accepted";
    case AuthoritativeMovementDecision::Clamped:
        return "Clamped";
    case AuthoritativeMovementDecision::RejectedByValidator:
        return "RejectedByValidator";
    }

    return "Unknown";
}

[[nodiscard]] Vec3 Convert(Vector3 vector)
{
    return Vec3{vector.x, vector.y, vector.z};
}

[[nodiscard]] Json BuildVectorJson(const Vec3& vector)
{
    return Json{{"x", vector.x}, {"y", vector.y}, {"z", vector.z}};
}

[[nodiscard]] Json BuildStringArrayJson(const std::vector<std::string>& values)
{
    Json array = Json::array();
    for (const auto& value : values)
    {
        array.push_back(value);
    }
    return array;
}

[[nodiscard]] Json BuildDiagnosticsJson(
    const std::vector<HeadlessEvaluationDiagnostic>& diagnostics)
{
    Json array = Json::array();
    for (const auto& diagnostic : diagnostics)
    {
        array.push_back(Json{
            {"severity", diagnostic.severity},
            {"code", diagnostic.code},
            {"message", diagnostic.message},
            {"path", diagnostic.path.generic_string()},
        });
    }
    return array;
}

[[nodiscard]] Json BuildReportJson(const HeadlessEvaluationReport& report)
{
    Json dirty = Json::array();
    for (const auto entityId : report.dirtyEntityIds)
    {
        dirty.push_back(entityId);
    }

    return Json{
        {"schemaVersion", report.schemaVersion},
        {"tool", report.tool},
        {"status", report.status},
        {"projectId", report.projectId},
        {"projectPath", report.projectPath.generic_string()},
        {"tickCount", report.tickCount},
        {"entityCount", report.entityCount},
        {"inputQueuedCount", report.inputQueuedCount},
        {"inputAcceptedCount", report.inputAcceptedCount},
        {"inputRejectedCount", report.inputRejectedCount},
        {"serverAuthority", report.serverAuthority},
        {"networkMode", report.networkMode},
        {"snapshotCount", report.snapshotCount},
        {"dirtyEntityIds", std::move(dirty)},
        {"initialPosition", BuildVectorJson(report.initialPosition)},
        {"beforeTickPosition", BuildVectorJson(report.beforeTickPosition)},
        {"finalPosition", BuildVectorJson(report.finalPosition)},
        {"authoritativeInitialState", BuildVectorJson(report.authoritativeInitialState)},
        {"authoritativeFinalState", BuildVectorJson(report.authoritativeFinalState)},
        {"invalidInputDiagnostics", BuildDiagnosticsJson(report.invalidInputDiagnostics)},
        {"nonClaims", BuildStringArrayJson(report.nonClaims)},
        {"diagnostics", BuildDiagnosticsJson(report.diagnostics)},
    };
}

[[nodiscard]] bool ValidateCommonOptions(
    const HeadlessEvaluationOptions& options,
    const char* diagnosticPrefix,
    std::vector<HeadlessEvaluationDiagnostic>& diagnostics)
{
    if (options.ticks <= 0)
    {
        diagnostics.push_back(MakeDiagnostic(
            std::string(diagnosticPrefix) + ".Evaluation.InvalidTickCount",
            "Headless evaluation tick count must be positive"));
    }
    if (!(options.fixedDtSeconds > 0.0f))
    {
        diagnostics.push_back(MakeDiagnostic(
            std::string(diagnosticPrefix) + ".Evaluation.InvalidFixedDelta",
            "Headless evaluation fixed delta must be positive"));
    }
    if (options.diagnosticsOut.empty())
    {
        diagnostics.push_back(MakeDiagnostic(
            std::string(diagnosticPrefix) + ".Evaluation.DiagnosticsPathMissing",
            "Diagnostics output directory is required"));
    }

    return diagnostics.empty();
}

[[nodiscard]] InputCommand MakeMovementCommand(
    std::uint64_t sequence,
    float moveX,
    float moveY,
    float moveZ = 0.0f)
{
    InputCommand command;
    command.sequence = sequence;
    command.clientTick = sequence;
    command.serverRecvTick = 1;
    command.moveX = moveX;
    command.moveY = moveY;
    command.moveZ = moveZ;
    command.dtMicros = 16'666;
    return command;
}

[[nodiscard]] HeadlessEvaluationReport RunStarterArenaServerSmoke(
    const HeadlessEvaluationOptions& options)
{
    HeadlessEvaluationReport report;
    report.projectPath = options.projectPath;
    report.tickCount = options.ticks;
    report.serverAuthority = true;
    report.networkMode = "HeadlessSmoke";
    report.nonClaims = {
        "Not full multiplayer",
        "Not MMO proof",
        "Not editor workflow",
        "Not package output",
        "Not Visual Blocks",
        "No external client process",
    };

    const auto manifestPath =
        ResolveManifestPath(options.projectPath, "StarterArena.sagaproj");
    auto projectId = ReadProjectId(
        manifestPath,
        "StarterArena",
        "StarterArena",
        report.diagnostics);
    if (!projectId.has_value())
    {
        return report;
    }
    report.projectId = *projectId;
    if (report.projectId != "starter-arena")
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "StarterArena.Project.ProjectIdMismatch",
            "StarterArena server smoke requires projectId starter-arena",
            manifestPath));
        return report;
    }

    if (!ValidateCommonOptions(options, "StarterArena", report.diagnostics))
    {
        return report;
    }

    AuthoritativeMovementCore movement;
    if (!movement.RegisterActor(kClientId, kEntityId, Vector3{0.0f, 0.0f, 0.0f}))
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "StarterArena.ServerSmoke.ActorRegistrationFailed",
            "Controlled actor registration failed"));
        return report;
    }
    report.entityCount = 1;

    const auto initial = movement.GetActorPosition(kEntityId);
    if (!initial.has_value())
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "StarterArena.ServerSmoke.ActorMissing",
            "Controlled actor was not available after registration"));
        return report;
    }
    report.initialPosition = Convert(*initial);
    report.authoritativeInitialState = report.initialPosition;

    const InputCommand validCommand = MakeMovementCommand(1, 1.0f, 0.0f);
    const auto enqueueResult =
        movement.EnqueueInput(kClientId, kEntityId, validCommand);
    if (enqueueResult.decision != AuthoritativeMovementDecision::Queued)
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "StarterArena.ServerSmoke.InputQueueFailed",
            "Valid movement input did not queue successfully"));
        return report;
    }
    report.inputQueuedCount = 1;

    const InputCommand invalidCommand = MakeMovementCommand(2, 2.0f, 0.0f);
    const auto invalidResult =
        movement.EnqueueInput(kClientId, kEntityId, invalidCommand);
    if (invalidResult.decision != AuthoritativeMovementDecision::RejectInput)
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "StarterArena.ServerSmoke.InvalidInputAccepted",
            std::string("Invalid movement input decision was ") +
                ToString(invalidResult.decision)));
        return report;
    }
    report.inputRejectedCount = 1;
    report.invalidInputDiagnostics.push_back(MakeInfoDiagnostic(
        "StarterArena.ServerSmoke.InvalidInputRejected",
        "Over-magnitude input was rejected before authoritative tick mutation"));

    const auto beforeTick = movement.GetActorPosition(kEntityId);
    if (!beforeTick.has_value())
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "StarterArena.ServerSmoke.ActorMissingBeforeTick",
            "Controlled actor was not available before tick"));
        return report;
    }
    report.beforeTickPosition = Convert(*beforeTick);

    for (int tick = 1; tick <= options.ticks; ++tick)
    {
        const auto tickReport =
            movement.Tick(static_cast<std::uint64_t>(tick), options.fixedDtSeconds);
        for (const auto& result : tickReport.results)
        {
            if (result.decision == AuthoritativeMovementDecision::Accepted ||
                result.decision == AuthoritativeMovementDecision::Clamped)
            {
                ++report.inputAcceptedCount;
            }
        }
        for (const auto entityId : tickReport.dirtyEntityIds)
        {
            report.dirtyEntityIds.push_back(entityId);
        }
    }
    report.snapshotCount = static_cast<int>(report.dirtyEntityIds.size());

    const auto final = movement.GetActorPosition(kEntityId);
    if (!final.has_value())
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "StarterArena.ServerSmoke.ActorMissingAfterTick",
            "Controlled actor was not available after tick"));
        return report;
    }
    report.finalPosition = Convert(*final);
    report.authoritativeFinalState = report.finalPosition;

    if (report.beforeTickPosition.x != report.initialPosition.x ||
        report.beforeTickPosition.y != report.initialPosition.y ||
        report.beforeTickPosition.z != report.initialPosition.z)
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "StarterArena.ServerSmoke.MutatedBeforeTick",
            "Controlled actor moved before the authoritative tick"));
        return report;
    }

    if (report.finalPosition.x == report.initialPosition.x &&
        report.finalPosition.y == report.initialPosition.y &&
        report.finalPosition.z == report.initialPosition.z)
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "StarterArena.ServerSmoke.NoMutationAfterTick",
            "Controlled actor did not move after the authoritative tick"));
        return report;
    }

    if (report.inputAcceptedCount != 1 ||
        report.inputRejectedCount != 1 ||
        report.snapshotCount != 1 ||
        report.invalidInputDiagnostics.empty())
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "StarterArena.ServerSmoke.AuthorityEvidenceIncomplete",
            "Server smoke did not record accepted input, rejected input, and snapshot evidence"));
        return report;
    }

    report.status = "Passed";
    return report;
}

} // namespace

[[nodiscard]] HeadlessEvaluationReport RunMultiplayerSandboxHeadlessEvaluation(
    const HeadlessEvaluationOptions& options)
{
    HeadlessEvaluationReport report;
    report.projectPath = options.projectPath;
    report.tickCount = options.ticks;
    report.serverAuthority = true;
    report.networkMode = "HeadlessSmoke";

    const auto manifestPath =
        ResolveManifestPath(options.projectPath, "MultiplayerSandbox.sagaproj");
    auto projectId = ReadProjectId(
        manifestPath,
        "MultiplayerSandbox",
        "MultiplayerSandbox",
        report.diagnostics);
    if (!projectId.has_value())
    {
        return report;
    }
    report.projectId = *projectId;

    if (!ValidateCommonOptions(options, "MultiplayerSandbox", report.diagnostics))
    {
        return report;
    }

    AuthoritativeMovementCore movement;
    if (!movement.RegisterActor(kClientId, kEntityId, Vector3{0.0f, 0.0f, 0.0f}))
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Evaluation.ActorRegistrationFailed",
            "Controlled actor registration failed"));
        return report;
    }
    report.entityCount = 1;

    const auto initial = movement.GetActorPosition(kEntityId);
    if (!initial.has_value())
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Evaluation.ActorMissing",
            "Controlled actor was not available after registration"));
        return report;
    }
    report.initialPosition = Convert(*initial);
    report.authoritativeInitialState = report.initialPosition;

    InputCommand command = MakeMovementCommand(1, 0.0f, 1.0f);

    const auto enqueueResult =
        movement.EnqueueInput(kClientId, kEntityId, command);
    if (enqueueResult.decision != AuthoritativeMovementDecision::Queued)
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Evaluation.InputQueueFailed",
            "Movement input did not queue successfully"));
        return report;
    }
    report.inputQueuedCount = 1;

    const auto beforeTick = movement.GetActorPosition(kEntityId);
    if (!beforeTick.has_value())
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Evaluation.ActorMissingBeforeTick",
            "Controlled actor was not available before tick"));
        return report;
    }
    report.beforeTickPosition = Convert(*beforeTick);

    for (int tick = 1; tick <= options.ticks; ++tick)
    {
        const auto tickReport =
            movement.Tick(static_cast<std::uint64_t>(tick), options.fixedDtSeconds);
        for (const auto& result : tickReport.results)
        {
            if (result.decision == AuthoritativeMovementDecision::Accepted ||
                result.decision == AuthoritativeMovementDecision::Clamped)
            {
                ++report.inputAcceptedCount;
            }
        }
        for (const auto entityId : tickReport.dirtyEntityIds)
        {
            report.dirtyEntityIds.push_back(entityId);
        }
    }

    const auto final = movement.GetActorPosition(kEntityId);
    if (!final.has_value())
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Evaluation.ActorMissingAfterTick",
            "Controlled actor was not available after tick"));
        return report;
    }
    report.finalPosition = Convert(*final);
    report.authoritativeFinalState = report.finalPosition;
    report.snapshotCount = static_cast<int>(report.dirtyEntityIds.size());

    if (report.beforeTickPosition.x != report.initialPosition.x ||
        report.beforeTickPosition.y != report.initialPosition.y ||
        report.beforeTickPosition.z != report.initialPosition.z)
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Evaluation.MutatedBeforeTick",
            "Controlled actor moved before the authoritative tick"));
        return report;
    }

    if (report.finalPosition.x == report.initialPosition.x &&
        report.finalPosition.y == report.initialPosition.y &&
        report.finalPosition.z == report.initialPosition.z)
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Evaluation.NoMutationAfterTick",
            "Controlled actor did not move after the authoritative tick"));
        return report;
    }

    report.status = "Passed";
    return report;
}

HeadlessEvaluationReport RunHeadlessEvaluation(const HeadlessEvaluationOptions& options)
{
    if (options.starterArenaServerSmoke)
    {
        return RunStarterArenaServerSmoke(options);
    }

    return RunMultiplayerSandboxHeadlessEvaluation(options);
}

bool WriteHeadlessEvaluationReportJson(
    const HeadlessEvaluationReport& report,
    const std::filesystem::path& outputPath,
    std::string& error)
{
    if (outputPath.empty())
    {
        error = "headless evaluation report output path is empty";
        return false;
    }

    std::error_code ec;
    const auto parent = outputPath.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, ec);
        if (ec)
        {
            error = "cannot create report output directory: " + ec.message();
            return false;
        }
    }

    std::ofstream output(outputPath);
    if (!output.is_open())
    {
        error = "cannot open headless evaluation report for writing";
        return false;
    }

    output << BuildReportJson(report).dump(2) << '\n';
    if (!output.good())
    {
        error = "failed writing headless evaluation report";
        return false;
    }
    return true;
}

} // namespace MultiplayerSandboxHeadless
