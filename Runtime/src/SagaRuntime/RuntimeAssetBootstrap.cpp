/// @file RuntimeAssetBootstrap.cpp
/// @brief Runtime package asset registry bootstrap facade implementation.

#include "SagaRuntime/RuntimeAssetBootstrap.hpp"

#include <SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.h>
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

[[nodiscard]] RuntimeAssetBootstrapDiagnostic ToRuntimeDiagnostic(
    const SagaEngine::Startup::RuntimeStartupGateDiagnostic& source)
{
    RuntimeAssetBootstrapDiagnostic diagnostic;
    diagnostic.severity = RuntimeAssetBootstrapDiagnosticSeverity::Error;
    diagnostic.category =
        RuntimeAssetBootstrapDiagnosticCategory::StartupValidation;
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

[[nodiscard]] RuntimeAssetBootstrapDiagnostic ToRuntimeDiagnostic(
    const SagaEngine::Resources::RuntimeAssetRegistryBootstrapDiagnostic& source)
{
    RuntimeAssetBootstrapDiagnostic diagnostic;
    diagnostic.severity = RuntimeAssetBootstrapDiagnosticSeverity::Error;
    diagnostic.category =
        RuntimeAssetBootstrapDiagnosticCategory::AssetRegistryBootstrap;
    diagnostic.diagnosticId = source.diagnosticId;
    diagnostic.message = source.message;
    diagnostic.manifestPath = source.manifestPath;
    diagnostic.referenceIndex = source.referenceIndex;
    diagnostic.itemIndex = source.assetIndex;
    if (!source.assetKey.empty())
    {
        diagnostic.resourceId = source.assetKey;
    }
    if (source.assetId != SagaEngine::Resources::kInvalidAssetId)
    {
        diagnostic.assetId = source.assetId;
    }
    diagnostic.resolvedPath = source.resolvedPath;
    return diagnostic;
}

} // namespace

RuntimeAssetBootstrapResult RuntimeAssetBootstrap::RegisterPackageAssets(
    SagaEngine::Resources::AssetRegistry& registry,
    const RuntimeAssetBootstrapOptions& options)
{
    RuntimeAssetBootstrapResult result;

    SagaEngine::Startup::RuntimeStartupGateOptions gateOptions;
    gateOptions.packageManifestPath = options.packageManifestPath;
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

    if (!gateResult.Succeeded())
    {
        return result;
    }

    SagaEngine::Resources::RuntimeAssetRegistryBootstrapOptions
        bootstrapOptions;
    bootstrapOptions.packageManifestPath = options.packageManifestPath;
    bootstrapOptions.packageBaseDirectory = options.packageBaseDirectory;
    bootstrapOptions.validateAssetFiles = options.validateAssetFiles;

    const SagaEngine::Resources::RuntimeAssetRegistryBootstrapResult
        bootstrapResult =
            SagaEngine::Resources::RuntimeAssetRegistryBootstrapper::
                RegisterPackageAssetsFromPackageIdentityManifest(
                    registry,
                    gateResult.packageManifest,
                    bootstrapOptions);

    result.registeredAssetCount = bootstrapResult.registeredAssetCount;
    result.diagnostics.reserve(
        result.diagnostics.size() + bootstrapResult.diagnostics.size());
    for (const SagaEngine::Resources::RuntimeAssetRegistryBootstrapDiagnostic&
             diagnostic : bootstrapResult.diagnostics)
    {
        result.diagnostics.push_back(ToRuntimeDiagnostic(diagnostic));
    }

    return result;
}

} // namespace SagaRuntime
