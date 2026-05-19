/// @file PublishBlockerKind.hpp
/// @brief Shared publish blocker kind vocabulary.

#pragma once

#include <cstdint>

namespace SagaShared::Publish
{

/// Stable publish blocker families used by readiness reports.
enum class PublishBlockerKind : std::uint8_t
{
    Unknown,
    ProjectManifestInvalid,
    SDECompileError,
    GraphValidationError,
    AuthorityValidationError,
    ScriptCompileError,
    AssetCookError,
    StaleArtifact,
    PackageManifestInvalid,
    CollaborationConflict,
    PermissionDenied,
    ToolchainInvalid,
    RuntimeManifestInvalid,
    ServerPackageInvalid,
    DiagnosticsFatal,
};

} // namespace SagaShared::Publish
