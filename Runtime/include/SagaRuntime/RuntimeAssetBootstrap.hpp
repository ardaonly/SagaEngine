/// @file RuntimeAssetBootstrap.hpp
/// @brief Runtime-owned package asset registry bootstrap facade.

#pragma once

#include <SagaRuntime/RuntimeStartupPreflight.hpp>

#include <SagaEngine/Resources/AssetRegistry.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaRuntime
{

/// Severity for Runtime asset bootstrap diagnostics.
enum class RuntimeAssetBootstrapDiagnosticSeverity
{
    Error = 0,
};

/// Runtime asset bootstrap diagnostic source category.
enum class RuntimeAssetBootstrapDiagnosticCategory
{
    StartupValidation = 0,
    AssetRegistryBootstrap,
};

/// Runtime-owned asset bootstrap options.
struct RuntimeAssetBootstrapOptions
{
    std::filesystem::path packageManifestPath;     ///< Package manifest to validate and register.
    std::filesystem::path packageBaseDirectory;    ///< Optional base for package-relative references.
    RuntimeStartupDomain expectedDomain = RuntimeStartupDomain::Client;
    bool validateReferencedManifestFiles = true;   ///< Validate package manifest references before registration.
    bool validateAssetFiles = true;                ///< Validate cooked asset files while bootstrapping.
    std::optional<std::string> expectedRuntimeCompatibilityVersion;
};

/// Runtime-facing diagnostic emitted while validating or bootstrapping package assets.
struct RuntimeAssetBootstrapDiagnostic
{
    RuntimeAssetBootstrapDiagnosticSeverity severity =
        RuntimeAssetBootstrapDiagnosticSeverity::Error;
    RuntimeAssetBootstrapDiagnosticCategory category =
        RuntimeAssetBootstrapDiagnosticCategory::StartupValidation;
    std::string diagnosticId;
    std::string message;
    std::filesystem::path manifestPath;
    std::optional<std::string> fieldName;
    std::optional<std::size_t> referenceIndex;
    std::optional<std::size_t> itemIndex;
    std::optional<std::string> resourceId;
    std::optional<SagaEngine::Resources::AssetId> assetId;
    std::optional<std::filesystem::path> resolvedPath;
};

/// Result of Runtime-owned package asset bootstrap.
struct RuntimeAssetBootstrapResult
{
    std::size_t registeredAssetCount = 0;                   ///< Number of assets inserted into the registry.
    std::vector<RuntimeAssetBootstrapDiagnostic> diagnostics; ///< Runtime-facing validation/bootstrap diagnostics.

    /// Return true when validation and registry bootstrap completed.
    [[nodiscard]] bool Succeeded() const noexcept { return diagnostics.empty(); }
};

/// Runtime facade for validating a package and registering its assets.
class RuntimeAssetBootstrap
{
public:
    /// Validate a package and register its identity-backed assets into the caller-owned registry.
    [[nodiscard]] static RuntimeAssetBootstrapResult RegisterPackageAssets(
        SagaEngine::Resources::AssetRegistry& registry,
        const RuntimeAssetBootstrapOptions& options);
};

} // namespace SagaRuntime
