/// @file SagaProductHost.cpp
/// @brief Product host boundary implementation for Saga role targets.

#include "App/SagaProductHost.h"

namespace SagaProduct
{
namespace
{

[[nodiscard]] bool RequiresStartupPackageManifest(
    SagaProductTargetKind target) noexcept
{
    return target == SagaProductTargetKind::Runtime;
}

[[nodiscard]] SagaProductDiagnostic MakeServerExecutionUnsupportedDiagnostic()
{
    SagaProductDiagnostic diagnostic;
    diagnostic.target = SagaProductTargetKind::Server;
    diagnostic.stage = SagaProductDiagnosticStage::TargetPreparation;
    diagnostic.diagnosticId =
        SagaProductDiagnostics::ServerExecutionUnsupported;
    diagnostic.message =
        "Product dedicated-server execution is not implemented.";
    return diagnostic;
}

[[nodiscard]] std::string MissingPackageManifestMessage(
    SagaProductTargetKind target)
{
    return std::string(ToString(target)) +
        " target requires --package-manifest";
}

[[nodiscard]] SagaProductDiagnostic MakeMissingPackageManifestDiagnostic(
    SagaProductTargetKind target)
{
    SagaProductDiagnostic diagnostic;
    diagnostic.target = target;
    diagnostic.stage = SagaProductDiagnosticStage::TargetPreparation;
    diagnostic.diagnosticId = SagaProductDiagnostics::PackageManifestMissing;
    diagnostic.message = MissingPackageManifestMessage(target);
    return diagnostic;
}

} // namespace

SagaPreparedTarget SagaProductHost::PrepareTarget(
    const SagaSessionModel& session) const
{
    SagaPreparedTarget prepared;
    prepared.kind = session.target;
    prepared.workspace = session.workspace;

    switch (session.target)
    {
        case SagaProductTargetKind::Editor:
            prepared.executableName = "SagaEditor";
            prepared.availability = SagaLaunchTargetAvailability::Available;
            prepared.arguments = {
                "--workspace",
                session.workspace.root.string(),
            };
            if (!session.workspace.editorProfile.empty())
            {
                prepared.arguments.push_back("--profile");
                prepared.arguments.push_back(session.workspace.editorProfile);
            }
            break;
        case SagaProductTargetKind::Runtime:
            prepared.executableName = "SagaRuntime";
            prepared.availability = SagaLaunchTargetAvailability::Bounded;
            if (session.packageManifestPath.has_value())
            {
                prepared.arguments.push_back("--package-manifest");
                prepared.arguments.push_back(session.packageManifestPath->string());
            }
            break;
        case SagaProductTargetKind::Server:
            prepared.availability = SagaLaunchTargetAvailability::Unsupported;
            break;
    }

    return prepared;
}

SagaTargetPreparationResult SagaProductHost::PrepareTargetWithDiagnostics(
    const SagaSessionModel& session) const
{
    SagaTargetPreparationResult result;
    result.target.kind = session.target;
    result.target.workspace = session.workspace;

    if (session.target == SagaProductTargetKind::Server)
    {
        result.diagnostics.push_back(
            MakeServerExecutionUnsupportedDiagnostic());
        return result;
    }

    if (RequiresStartupPackageManifest(session.target) &&
        !session.packageManifestPath.has_value())
    {
        result.diagnostics.push_back(
            MakeMissingPackageManifestDiagnostic(session.target));
        return result;
    }

    result.target = PrepareTarget(session);
    result.ok = true;
    return result;
}

} // namespace SagaProduct
