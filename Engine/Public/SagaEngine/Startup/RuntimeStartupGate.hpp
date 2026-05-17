/// @file RuntimeStartupGate.hpp
/// @brief Thin startup validation orchestrator for runtime package manifests.

#pragma once

#include <SagaEngine/Packages/PackageManifest.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaEngine::Startup
{

/// Expected startup execution domain for a runtime package.
enum class RuntimeStartupDomain
{
    Client = 0,
    Server,
};

/// Caller-supplied policy for startup package validation.
struct RuntimeStartupGateOptions
{
    std::filesystem::path packageManifestPath;                       ///< Package manifest to validate.
    RuntimeStartupDomain expectedDomain = RuntimeStartupDomain::Client; ///< Expected runtime/server domain.
    bool validateReferencedManifestFiles = true;                     ///< Validate staged manifest references.
    bool validateAssetFiles = true;                                  ///< Validate cooked asset files through asset loader policy.
    std::optional<std::string> expectedRuntimeCompatibilityVersion;  ///< Expected compatibility token supplied by caller.
};

/// Normalized startup validation diagnostic.
struct RuntimeStartupGateDiagnostic
{
    std::string diagnosticId;                            ///< Stable subsystem diagnostic id.
    std::string message;                                 ///< Human-readable diagnostic message.
    std::filesystem::path manifestPath;                  ///< Manifest that emitted or owns the diagnostic.
    std::optional<std::string> fieldName;                ///< Field involved in the diagnostic when available.
    std::optional<std::size_t> referenceIndex;           ///< Package manifest reference index when applicable.
    std::optional<std::size_t> itemIndex;                ///< Asset/artifact item index when applicable.
    std::optional<std::string> resourceId;               ///< Package, asset, or artifact id when available.
    std::optional<std::filesystem::path> resolvedPath;   ///< Resolved path involved in the diagnostic when relevant.
};

/// Result of the thin runtime startup validation gate.
struct RuntimeStartupGateResult
{
    Packages::PackageManifest packageManifest;               ///< Parsed package manifest when package loading succeeds.
    std::vector<RuntimeStartupGateDiagnostic> diagnostics;    ///< Normalized startup diagnostics.

    /// Return true when the startup package is accepted.
    [[nodiscard]] bool Succeeded() const noexcept { return diagnostics.empty(); }
};

/// Orchestrates startup manifest validation without loading runtime resources.
class RuntimeStartupGate
{
public:
    /// Validate package, referenced manifests, and compatibility policy for startup.
    [[nodiscard]] static RuntimeStartupGateResult ValidatePackageForStartup(
        const RuntimeStartupGateOptions& options);
};

} // namespace SagaEngine::Startup
