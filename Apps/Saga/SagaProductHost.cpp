/// @file SagaProductHost.cpp
/// @brief Product host boundary implementation for Saga role targets.

#include "SagaProductHost.h"

namespace SagaProduct
{
namespace
{

[[nodiscard]] bool RequiresStartupPackageManifest(
    SagaProductTargetKind target) noexcept
{
    return target == SagaProductTargetKind::Runtime ||
        target == SagaProductTargetKind::Server;
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
    diagnostic.phase = SagaProductDiagnosticPhase::TargetPreparation;
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
            prepared.moduleName = "SagaEditorModule";
            break;
        case SagaProductTargetKind::Runtime:
            prepared.moduleName = "SagaRuntimeModule";
            prepared.executableName = "SagaRuntime";
            prepared.sameProcess = false;
            if (session.packageManifestPath.has_value())
            {
                prepared.arguments.push_back("--package-manifest");
                prepared.arguments.push_back(session.packageManifestPath->string());
            }
            break;
        case SagaProductTargetKind::Server:
            prepared.moduleName = "SagaServerModule";
            prepared.executableName = "SagaServer";
            prepared.sameProcess = false;
            if (session.packageManifestPath.has_value())
            {
                prepared.arguments.push_back("--package-manifest");
                prepared.arguments.push_back(session.packageManifestPath->string());
            }
            break;
    }
    if (prepared.executableName.empty())
    {
        prepared.executableName = "Saga";
    }

    return prepared;
}

SagaTargetPreparationResult SagaProductHost::PrepareTargetWithDiagnostics(
    const SagaSessionModel& session) const
{
    SagaTargetPreparationResult result;
    result.target.kind = session.target;
    result.target.workspace = session.workspace;

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
