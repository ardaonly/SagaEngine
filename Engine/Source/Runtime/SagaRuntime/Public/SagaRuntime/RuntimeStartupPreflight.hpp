/// @file RuntimeStartupPreflight.hpp
/// @brief Runtime-owned startup package preflight facade.

#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaRuntime
{

/// Expected startup execution domain for a runtime package.
enum class RuntimeStartupDomain
{
    Client = 0,
    Server,
};

/// Runtime-owned startup preflight options.
struct RuntimeStartupPreflightOptions
{
    std::optional<std::filesystem::path> packageManifestPath; ///< Package manifest to validate when supplied.
    std::filesystem::path packageBaseDirectory;               ///< Optional base for package-relative references.
    RuntimeStartupDomain expectedDomain = RuntimeStartupDomain::Client;
    bool allowMissingPackageManifestForDev = true;            ///< Preserve dev launches without a package.
    bool validateReferencedManifestFiles = true;              ///< Validate staged manifest references.
    bool validateAssetFiles = true;                           ///< Validate cooked asset files.
    std::optional<std::string> expectedRuntimeCompatibilityVersion;
};

/// Runtime-owned startup preflight diagnostic.
struct RuntimeStartupPreflightDiagnostic
{
    std::string diagnosticId;
    std::string message;
    std::filesystem::path manifestPath;
    std::optional<std::string> fieldName;
    std::optional<std::size_t> referenceIndex;
    std::optional<std::size_t> itemIndex;
    std::optional<std::string> resourceId;
    std::optional<std::filesystem::path> resolvedPath;
};

/// Runtime-owned startup preflight result.
struct RuntimeStartupPreflightResult
{
    bool validationSkipped = false;
    std::vector<RuntimeStartupPreflightDiagnostic> diagnostics;

    /// Return true when startup preflight accepts the package or intentionally skipped validation.
    [[nodiscard]] bool Succeeded() const noexcept { return diagnostics.empty(); }
};

/// Runtime facade for startup package preflight.
class RuntimeStartupPreflight
{
public:
    /// Validate the runtime startup package through the Runtime-owned facade.
    [[nodiscard]] static RuntimeStartupPreflightResult ValidatePackageForStartup(
        const RuntimeStartupPreflightOptions& options);
};

namespace RuntimeStartupPreflightDiagnostics
{

inline constexpr const char* PackageManifestMissing =
    "Runtime.StartupPreflight.PackageManifestMissing";

} // namespace RuntimeStartupPreflightDiagnostics

} // namespace SagaRuntime
