/// @file RuntimeStartupPreflight.cpp
/// @brief Runtime startup preflight facade implementation.

#include "SagaRuntime/RuntimeStartupPreflight.hpp"

#include <SagaEngine/Startup/RuntimeStartupGate.hpp>

#include <utility>

namespace SagaRuntime
{
namespace
{

[[nodiscard]] SagaEngine::Startup::RuntimeStartupDomain ToEngineDomain(
    RuntimeStartupDomain domain) noexcept
{
    switch (domain)
    {
        case RuntimeStartupDomain::Client:
            return SagaEngine::Startup::RuntimeStartupDomain::Client;
        case RuntimeStartupDomain::Server:
            return SagaEngine::Startup::RuntimeStartupDomain::Server;
    }

    return SagaEngine::Startup::RuntimeStartupDomain::Client;
}

[[nodiscard]] RuntimeStartupPreflightDiagnostic ToRuntimeDiagnostic(
    const SagaEngine::Startup::RuntimeStartupGateDiagnostic& source)
{
    RuntimeStartupPreflightDiagnostic diagnostic;
    diagnostic.diagnosticId = source.diagnosticId;
    diagnostic.message = source.message;
    diagnostic.manifestPath = source.manifestPath;
    diagnostic.fieldName = source.fieldName;
    diagnostic.referenceIndex = source.referenceIndex;
    diagnostic.itemIndex = source.itemIndex;
    diagnostic.resourceId = source.resourceId;
    diagnostic.resolvedPath = source.resolvedPath;
    return diagnostic;
}

[[nodiscard]] RuntimeStartupPreflightDiagnostic MakeMissingPackageDiagnostic()
{
    RuntimeStartupPreflightDiagnostic diagnostic;
    diagnostic.diagnosticId =
        RuntimeStartupPreflightDiagnostics::PackageManifestMissing;
    diagnostic.message = "Runtime startup requires a package manifest.";
    return diagnostic;
}

} // namespace

RuntimeStartupPreflightResult RuntimeStartupPreflight::ValidatePackageForStartup(
    const RuntimeStartupPreflightOptions& options)
{
    RuntimeStartupPreflightResult result;

    if (!options.packageManifestPath.has_value())
    {
        if (options.allowMissingPackageManifestForDev)
        {
            result.validationSkipped = true;
        }
        else
        {
            result.diagnostics.push_back(MakeMissingPackageDiagnostic());
        }
        return result;
    }

    SagaEngine::Startup::RuntimeStartupGateOptions gateOptions;
    gateOptions.packageManifestPath = *options.packageManifestPath;
    gateOptions.packageBaseDirectory = options.packageBaseDirectory;
    gateOptions.expectedDomain = ToEngineDomain(options.expectedDomain);
    gateOptions.validateReferencedManifestFiles =
        options.validateReferencedManifestFiles;
    gateOptions.validateAssetFiles = options.validateAssetFiles;
    gateOptions.expectedRuntimeCompatibilityVersion =
        options.expectedRuntimeCompatibilityVersion;

    const SagaEngine::Startup::RuntimeStartupGateResult gateResult =
        SagaEngine::Startup::RuntimeStartupGate::ValidatePackageForStartup(
            gateOptions);
    result.diagnostics.reserve(gateResult.diagnostics.size());
    for (const SagaEngine::Startup::RuntimeStartupGateDiagnostic& diagnostic :
         gateResult.diagnostics)
    {
        result.diagnostics.push_back(ToRuntimeDiagnostic(diagnostic));
    }

    return result;
}

} // namespace SagaRuntime
