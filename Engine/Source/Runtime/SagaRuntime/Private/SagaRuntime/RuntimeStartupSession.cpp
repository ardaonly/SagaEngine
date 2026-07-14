/// @file RuntimeStartupSession.cpp
/// @brief Runtime startup session preparation facade implementation.

#include "SagaRuntime/RuntimeStartupSession.hpp"

namespace SagaRuntime
{

RuntimeStartupReport RuntimeStartupSession::Prepare(
    const RuntimeLaunchOptions& options)
{
    RuntimeStartupReport report;
    report.sessionDescriptor.serverHost = options.serverHost;
    report.sessionDescriptor.serverPort = options.serverPort;
    report.sessionDescriptor.headless = options.headless;
    report.sessionDescriptor.enableStartupUi = options.enableStartupUi;
    report.sessionDescriptor.startupContentRoot = options.startupContentRoot;
    report.sessionDescriptor.packageManifestPath = options.packageManifestPath;
    report.sessionDescriptor.packageBaseDirectory = options.packageBaseDirectory;
    report.sessionDescriptor.domain = options.expectedDomain;

    RuntimeStartupPreflightOptions preflightOptions;
    preflightOptions.packageManifestPath = options.packageManifestPath;
    preflightOptions.packageBaseDirectory = options.packageBaseDirectory;
    preflightOptions.expectedDomain = options.expectedDomain;
    preflightOptions.allowMissingPackageManifestForDev =
        options.allowMissingPackageManifestForDev;
    preflightOptions.validateReferencedManifestFiles =
        options.validateReferencedManifestFiles;
    preflightOptions.validateAssetFiles = options.validateAssetFiles;
    preflightOptions.expectedRuntimeCompatibilityVersion =
        options.expectedRuntimeCompatibilityVersion;

    report.preflight =
        RuntimeStartupPreflight::ValidatePackageForStartup(preflightOptions);
    return report;
}

} // namespace SagaRuntime
