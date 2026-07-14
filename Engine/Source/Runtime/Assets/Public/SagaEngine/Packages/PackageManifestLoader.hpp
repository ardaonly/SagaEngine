/// @file PackageManifestLoader.hpp
/// @brief Loads and validates runtime package manifest files.

#pragma once

#include <SagaEngine/Packages/PackageManifest.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaEngine::Packages
{

/// Structured validation error emitted while reading a package manifest.
struct PackageManifestError
{
    std::string diagnosticId;                         ///< Stable Runtime.Package.* diagnostic id.
    std::string message;                              ///< Human-readable validation message.
    std::filesystem::path manifestPath;               ///< Manifest file that produced the error.
    std::optional<std::size_t> referenceIndex;        ///< Manifest reference index when applicable.
    std::optional<std::string> fieldName;             ///< Field involved in the validation failure.
};

/// Result of a runtime package manifest load attempt.
struct PackageManifestLoadResult
{
    PackageManifest manifest;                    ///< Parsed data; only complete when Succeeded is true.
    std::vector<PackageManifestError> errors;    ///< Structured validation and I/O failures.

    /// Return true when the manifest loaded without validation errors.
    [[nodiscard]] bool Succeeded() const noexcept { return errors.empty(); }
};

/// Optional validation controls for runtime package manifest loading.
struct PackageManifestLoadOptions
{
    bool validateReferencedManifestFiles = false;      ///< Check referenced manifest files exist.
    std::filesystem::path packageBaseDirectory;        ///< Base path for relative manifest references.
};

/// Reads package manifests without owning build, cook, or package staging.
class PackageManifestLoader
{
public:
    /// Load and validate a package manifest from disk.
    [[nodiscard]] static PackageManifestLoadResult LoadFromFile(
        const std::filesystem::path& manifestPath);

    /// Load and validate a package manifest using explicit validation options.
    [[nodiscard]] static PackageManifestLoadResult LoadFromFile(
        const std::filesystem::path& manifestPath,
        const PackageManifestLoadOptions& options);
};

namespace PackageManifestDiagnostics
{

inline constexpr const char* ManifestMissing =
    "Runtime.Package.ManifestMissing";
inline constexpr const char* ParseFailed =
    "Runtime.Package.ParseFailed";
inline constexpr const char* UnsupportedVersion =
    "Runtime.Package.UnsupportedVersion";
inline constexpr const char* MissingField =
    "Runtime.Package.MissingField";
inline constexpr const char* InvalidField =
    "Runtime.Package.InvalidField";
inline constexpr const char* UnknownKind =
    "Runtime.Package.UnknownKind";
inline constexpr const char* DuplicateId =
    "Runtime.Package.DuplicateId";
inline constexpr const char* InvalidPath =
    "Runtime.Package.InvalidPath";
inline constexpr const char* FileMissing =
    "Runtime.Package.FileMissing";

} // namespace PackageManifestDiagnostics

} // namespace SagaEngine::Packages
