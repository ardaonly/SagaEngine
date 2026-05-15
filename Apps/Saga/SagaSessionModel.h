/// @file SagaSessionModel.h
/// @brief Product-level workspace session and launch target data.

#pragma once

#include <filesystem>
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

[[nodiscard]] const char* ToString(SagaProductTargetKind kind) noexcept;
[[nodiscard]] bool ParseTargetKind(const std::string& text,
                                   SagaProductTargetKind& out) noexcept;

} // namespace SagaProduct
