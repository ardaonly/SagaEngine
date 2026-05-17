/// @file SagaSessionModel.h
/// @brief Product-level workspace session and launch target data.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaProduct
{

/// Product role that the Saga orchestration layer can prepare.
enum class SagaProductTargetKind
{
    Editor,
    Runtime,
    Server,
};

/// SDE-backed workspace definition consumed by product startup.
struct SagaWorkspaceDefinition
{
    std::string           id;             ///< Stable workspace id from SDE data.
    std::string           displayName;    ///< Human-readable workspace label.
    std::filesystem::path root;           ///< Root directory of the validated SDE project.
    std::string           editorProfile;  ///< Editor profile id requested by the workspace.
    std::string           runtimeRole;    ///< Runtime role name declared by the workspace.
    std::string           serverRole;     ///< Server role name declared by the workspace.
    std::string           artifactHash;   ///< SDE artifact hash for deterministic identity.
};

/// Prepared product session after workspace validation.
struct SagaSessionModel
{
    SagaWorkspaceDefinition workspace; ///< Validated workspace contract.
    SagaProductTargetKind   target = SagaProductTargetKind::Editor;
    std::optional<std::filesystem::path> packageManifestPath; ///< Explicit startup package manifest for runtime/server.
};

/// Product phase that produced a preparation/startup diagnostic.
enum class SagaProductDiagnosticPhase
{
    Config = 0,
    WorkspaceResolution,
    TargetPreparation,
    StartupHandoff,
};

/// Narrow product-local diagnostic for target preparation/startup failures.
struct SagaProductDiagnostic
{
    SagaProductTargetKind target = SagaProductTargetKind::Editor;          ///< Product role affected by the diagnostic.
    SagaProductDiagnosticPhase phase = SagaProductDiagnosticPhase::Config; ///< Product phase that produced the diagnostic.
    std::string diagnosticId;                                              ///< Stable product-local diagnostic id.
    std::string message;                                                   ///< Human-readable diagnostic message.
    std::optional<std::filesystem::path> path;                             ///< Optional path related to the diagnostic.
};

/// Boundary-level same-process module preparation result.
struct SagaPreparedTarget
{
    SagaProductTargetKind    kind = SagaProductTargetKind::Editor;
    std::string              moduleName;
    std::string              executableName;
    std::vector<std::string> arguments;
    bool                     sameProcess = true;
    SagaWorkspaceDefinition  workspace;
};

/// Result of preparing a product target boundary.
struct SagaTargetPreparationResult
{
    bool ok = false;                                ///< True when target preparation can continue.
    SagaPreparedTarget target;                      ///< Prepared target when ok is true.
    std::vector<SagaProductDiagnostic> diagnostics; ///< Product-local preparation diagnostics.
};

namespace SagaProductDiagnostics
{

inline constexpr const char* PackageManifestMissing =
    "Saga.Target.PackageManifestMissing";
inline constexpr const char* ProcessStartFailed =
    "Saga.Target.ProcessStartFailed";
inline constexpr const char* ProcessExitedWithFailure =
    "Saga.Target.ProcessExitedWithFailure";

} // namespace SagaProductDiagnostics

[[nodiscard]] const char* ToString(SagaProductTargetKind kind) noexcept;
[[nodiscard]] bool ParseTargetKind(const std::string& text,
                                   SagaProductTargetKind& out) noexcept;
[[nodiscard]] const char* ToString(SagaProductDiagnosticPhase phase) noexcept;

} // namespace SagaProduct
