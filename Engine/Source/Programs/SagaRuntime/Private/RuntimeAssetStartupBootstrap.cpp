/// @file RuntimeAssetStartupBootstrap.cpp
/// @brief App-local Runtime asset bootstrap helper implementation.

#include "RuntimeAssetStartupBootstrap.hpp"

#include <SagaRuntime/RuntimeAssetBootstrap.hpp>

namespace SagaRuntimeApp
{

RuntimeAssetStartupBootstrapResult RuntimeAssetStartupBootstrap::Bootstrap(
    const SagaRuntime::RuntimeSessionDescriptor& descriptor,
    SagaEngine::Resources::AssetRegistry& registry)
{
    RuntimeAssetStartupBootstrapResult result;
    if (!descriptor.packageManifestPath.has_value())
    {
        result.bootstrapSkipped = true;
        result.summary =
            SagaRuntime::RuntimeAssetBootstrapDiagnostics::Summarize(
                result.bootstrap);
        return result;
    }

    SagaRuntime::RuntimeAssetBootstrapOptions options;
    options.packageManifestPath = *descriptor.packageManifestPath;
    options.packageBaseDirectory = descriptor.packageBaseDirectory;
    options.expectedDomain = descriptor.domain;

    result.bootstrap =
        SagaRuntime::RuntimeAssetBootstrap::RegisterPackageAssets(
            registry,
            options);
    result.summary =
        SagaRuntime::RuntimeAssetBootstrapDiagnostics::Summarize(
            result.bootstrap,
            options);
    result.diagnostics =
        SagaRuntime::RuntimeAssetBootstrapDiagnostics::BuildDiagnosticViews(
            result.bootstrap);
    return result;
}

} // namespace SagaRuntimeApp
