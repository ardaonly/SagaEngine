/// @file HeadlessPreview.cpp
/// @brief Implements bounded MultiplayerSandbox server-side preview proof.

#include "MultiplayerSandboxHeadless/HeadlessPreview.hpp"

#include "SagaServer/Simulation/AuthoritativeMovementCore.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <optional>
#include <system_error>

namespace MultiplayerSandboxHeadless
{
namespace
{

using Json = nlohmann::ordered_json;
using SagaEngine::Networking::ClientId;
using SagaEngine::Server::Simulation::AuthoritativeMovementCore;
using SagaEngine::Server::Simulation::AuthoritativeMovementDecision;
using SagaEngine::Server::Simulation::EntityId;
using SagaEngine::Server::Simulation::InputCommand;
using SagaEngine::Server::Simulation::Vector3;

constexpr ClientId kClientId = 1;
constexpr EntityId kEntityId = 1001;

[[nodiscard]] HeadlessPreviewDiagnostic MakeDiagnostic(
    std::string code,
    std::string message,
    std::filesystem::path path = {})
{
    HeadlessPreviewDiagnostic diagnostic;
    diagnostic.code = std::move(code);
    diagnostic.message = std::move(message);
    diagnostic.path = std::move(path);
    return diagnostic;
}

[[nodiscard]] std::filesystem::path ResolveManifestPath(
    const std::filesystem::path& projectPath)
{
    if (projectPath.empty())
    {
        return {};
    }
    if (projectPath.extension() == ".sagaproj")
    {
        return projectPath;
    }
    return projectPath / "MultiplayerSandbox.sagaproj";
}

[[nodiscard]] std::optional<std::string> ReadProjectId(
    const std::filesystem::path& manifestPath,
    std::vector<HeadlessPreviewDiagnostic>& diagnostics)
{
    if (!std::filesystem::exists(manifestPath))
    {
        diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Project.ManifestMissing",
            "MultiplayerSandbox .sagaproj manifest is missing",
            manifestPath));
        return std::nullopt;
    }

    std::ifstream input(manifestPath);
    if (!input.is_open())
    {
        diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Project.ManifestOpenFailed",
            "MultiplayerSandbox .sagaproj manifest cannot be opened",
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
            "MultiplayerSandbox.Project.ManifestParseFailed",
            std::string("MultiplayerSandbox .sagaproj JSON is invalid: ") + e.what(),
            manifestPath));
        return std::nullopt;
    }

    if (!json.contains("projectId") || !json["projectId"].is_string())
    {
        diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Project.ProjectIdMissing",
            "MultiplayerSandbox .sagaproj must contain string projectId",
            manifestPath));
        return std::nullopt;
    }

    return json["projectId"].get<std::string>();
}

[[nodiscard]] Vec3 Convert(Vector3 vector)
{
    return Vec3{vector.x, vector.y, vector.z};
}

[[nodiscard]] Json BuildVectorJson(const Vec3& vector)
{
    return Json{{"x", vector.x}, {"y", vector.y}, {"z", vector.z}};
}

[[nodiscard]] Json BuildDiagnosticsJson(
    const std::vector<HeadlessPreviewDiagnostic>& diagnostics)
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

[[nodiscard]] Json BuildReportJson(const HeadlessPreviewReport& report)
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
        {"dirtyEntityIds", std::move(dirty)},
        {"initialPosition", BuildVectorJson(report.initialPosition)},
        {"beforeTickPosition", BuildVectorJson(report.beforeTickPosition)},
        {"finalPosition", BuildVectorJson(report.finalPosition)},
        {"diagnostics", BuildDiagnosticsJson(report.diagnostics)},
    };
}

} // namespace

HeadlessPreviewReport RunHeadlessPreview(const HeadlessPreviewOptions& options)
{
    HeadlessPreviewReport report;
    report.projectPath = options.projectPath;
    report.tickCount = options.ticks;

    const auto manifestPath = ResolveManifestPath(options.projectPath);
    auto projectId = ReadProjectId(manifestPath, report.diagnostics);
    if (!projectId.has_value())
    {
        return report;
    }
    report.projectId = *projectId;

    if (options.ticks <= 0)
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Preview.InvalidTickCount",
            "Headless preview tick count must be positive"));
        return report;
    }
    if (!(options.fixedDtSeconds > 0.0f))
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Preview.InvalidFixedDelta",
            "Headless preview fixed delta must be positive"));
        return report;
    }

    AuthoritativeMovementCore movement;
    if (!movement.RegisterActor(kClientId, kEntityId, Vector3{0.0f, 0.0f, 0.0f}))
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Preview.ActorRegistrationFailed",
            "Controlled actor registration failed"));
        return report;
    }
    report.entityCount = 1;

    const auto initial = movement.GetActorPosition(kEntityId);
    if (!initial.has_value())
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Preview.ActorMissing",
            "Controlled actor was not available after registration"));
        return report;
    }
    report.initialPosition = Convert(*initial);

    InputCommand command;
    command.sequence = 1;
    command.clientTick = 1;
    command.serverRecvTick = 1;
    command.moveX = 0.0f;
    command.moveY = 1.0f;
    command.dtMicros = 16'666;

    const auto enqueueResult =
        movement.EnqueueInput(kClientId, kEntityId, command);
    if (enqueueResult.decision != AuthoritativeMovementDecision::Queued)
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Preview.InputQueueFailed",
            "Movement input did not queue successfully"));
        return report;
    }
    report.inputQueuedCount = 1;

    const auto beforeTick = movement.GetActorPosition(kEntityId);
    if (!beforeTick.has_value())
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Preview.ActorMissingBeforeTick",
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
            "MultiplayerSandbox.Preview.ActorMissingAfterTick",
            "Controlled actor was not available after tick"));
        return report;
    }
    report.finalPosition = Convert(*final);

    if (report.beforeTickPosition.x != report.initialPosition.x ||
        report.beforeTickPosition.y != report.initialPosition.y ||
        report.beforeTickPosition.z != report.initialPosition.z)
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Preview.MutatedBeforeTick",
            "Controlled actor moved before the authoritative tick"));
        return report;
    }

    if (report.finalPosition.x == report.initialPosition.x &&
        report.finalPosition.y == report.initialPosition.y &&
        report.finalPosition.z == report.initialPosition.z)
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Preview.NoMutationAfterTick",
            "Controlled actor did not move after the authoritative tick"));
        return report;
    }

    if (options.diagnosticsOut.empty())
    {
        report.diagnostics.push_back(MakeDiagnostic(
            "MultiplayerSandbox.Preview.DiagnosticsPathMissing",
            "Diagnostics output directory is required"));
        return report;
    }

    report.status = "Passed";
    return report;
}

bool WriteHeadlessPreviewReportJson(
    const HeadlessPreviewReport& report,
    const std::filesystem::path& outputPath,
    std::string& error)
{
    if (outputPath.empty())
    {
        error = "headless preview report output path is empty";
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
        error = "cannot open headless preview report for writing";
        return false;
    }

    output << BuildReportJson(report).dump(2) << '\n';
    if (!output.good())
    {
        error = "failed writing headless preview report";
        return false;
    }
    return true;
}

} // namespace MultiplayerSandboxHeadless
