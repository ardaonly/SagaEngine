/// @file RuntimeStartupSession.hpp
/// @brief Runtime-owned startup session preparation facade.

#pragma once

#include <SagaRuntime/RuntimeStartupPreflight.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace SagaRuntime
{

/// Normalized runtime startup/session intent supplied by an app entrypoint.
struct RuntimeLaunchOptions
{
    std::string serverHost = "127.0.0.1";
    std::uint16_t serverPort = 7777;
    bool headless = false;
    bool enableStartupUi = true;
    std::filesystem::path startupContentRoot;

    std::optional<std::filesystem::path> packageManifestPath;
    std::filesystem::path packageBaseDirectory;
    RuntimeStartupDomain expectedDomain = RuntimeStartupDomain::Client;
    bool allowMissingPackageManifestForDev = true;
    bool validateReferencedManifestFiles = true;
    bool validateAssetFiles = true;
    std::optional<std::string> expectedRuntimeCompatibilityVersion;
};

/// Runtime-owned descriptor prepared from normalized launch intent.
struct RuntimeSessionDescriptor
{
    std::string serverHost = "127.0.0.1";
    std::uint16_t serverPort = 7777;
    bool headless = false;
    bool enableStartupUi = true;
    std::filesystem::path startupContentRoot;
    std::optional<std::filesystem::path> packageManifestPath;
    std::filesystem::path packageBaseDirectory;
    RuntimeStartupDomain domain = RuntimeStartupDomain::Client;
};

/// Runtime startup preparation report.
struct RuntimeStartupReport
{
    RuntimeSessionDescriptor sessionDescriptor;
    RuntimeStartupPreflightResult preflight;

    /// Return true when startup preparation accepts the launch intent.
    [[nodiscard]] bool Succeeded() const noexcept { return preflight.Succeeded(); }
};

/// Runtime facade for preparing startup/session descriptors.
class RuntimeStartupSession
{
public:
    /// Prepare a runtime session descriptor and validate startup preflight.
    [[nodiscard]] static RuntimeStartupReport Prepare(
        const RuntimeLaunchOptions& options);
};

} // namespace SagaRuntime
