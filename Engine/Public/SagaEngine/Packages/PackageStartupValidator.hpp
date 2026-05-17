/// @file PackageStartupValidator.hpp
/// @brief Applies runtime startup acceptance policy to package manifests.

#pragma once

#include <SagaEngine/Packages/PackageManifestLoader.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaEngine::Packages
{

/// Runtime package validation controls for startup.
struct PackageStartupValidationOptions
{
    std::optional<PackageKind> expectedPackageKind;             ///< Reject packages for the wrong execution domain.
    std::optional<std::string> runtimeCompatibilityVersion;     ///< Reject packages built for a different runtime token.
    bool validateReferencedManifestFiles = true;                ///< Check asset and artifact manifest references exist.
    std::filesystem::path packageBaseDirectory;                 ///< Base path for relative manifest references.
};

/// Result of runtime package startup validation.
struct PackageStartupValidationResult
{
    PackageManifest manifest;                  ///< Parsed manifest returned by the loader.
    std::vector<PackageManifestError> errors;  ///< Loader and startup policy failures.

    /// Return true when startup policy accepted the package manifest.
    [[nodiscard]] bool Succeeded() const noexcept { return errors.empty(); }
};

/// Validates package manifests before runtime or server startup consumes them.
class PackageStartupValidator
{
public:
    /// Load and validate a package manifest for runtime startup.
    [[nodiscard]] static PackageStartupValidationResult ValidateManifestForStartup(
        const std::filesystem::path& manifestPath);

    /// Load and validate a package manifest for runtime startup with explicit policy.
    [[nodiscard]] static PackageStartupValidationResult ValidateManifestForStartup(
        const std::filesystem::path& manifestPath,
        const PackageStartupValidationOptions& options);
};

namespace PackageStartupDiagnostics
{

inline constexpr const char* WrongPackageKind =
    "Runtime.Package.WrongPackageKind";
inline constexpr const char* IncompatibleRuntime =
    "Runtime.Package.IncompatibleRuntime";

} // namespace PackageStartupDiagnostics

} // namespace SagaEngine::Packages
